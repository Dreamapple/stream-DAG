#pragma once
#include "stream-dag.h"
#include <brpc/channel.h>
#include <brpc/progressive_reader.h>

namespace stream_dag {

using json = nlohmann::json;

struct HttpRequest {
    std::string method;
    std::string url;
    std::map<std::string, std::string> params;
    std::map<std::string, std::string> headers;
    json data;

    bool stream = false; // 流式读取数据
    int timeout_ms = 0; // 超时时间，单位ms
};

class HttpResponse {
public:
    int status_code;
    std::map<std::string, std::string> headers;
    json body;
};

class StreamHttpReader : public brpc::ProgressiveReader {
public:
    StreamHttpReader(Stream<std::string>& body) : body_(body) {}

    butil::Status OnReadOnePart (const void* data, size_t length) override {
        return body_.append(std::string((const char*)data, length));
    }

    void OnEndOfMessage (const butil::Status& status) override {
        body_.half_close(status);
        delete this;
    }

private:
    Stream<std::string>& body_;
};

// ref https://github.com/apache/brpc/blob/master/docs/cn/http_client.md
class HttpNode : public BaseNode {
public:
    Status init(json& option) {
        std::string host = option.value("host", "");
        std::string lb = option.value("lb", "");
        int timeout_ms = option.value("timeout_ms", 20000);
        int max_retry = option.value("max_retry", 3);

        brpc::ChannelOptions opt;
        opt.protocol = brpc::PROTOCOL_HTTP;
        if (timeout_ms > 0) { opt.timeout_ms = timeout_ms; }
        if (max_retry > 0) { opt.max_retry = max_retry; }
        return (0 == chann_.Init(host.c_str(), lb.c_str(), &opt)) ? Status::OK() : Status(-1, "init channnel failed");
    }

    Status run(InputData<HttpRequest>& request, OutputData<HttpResponse>& response, Stream<std::string>& body) {
        // 这里面没有必要写异步任务
        // 直接同步执行, 阻塞时 brpc 会自动调度到其它bthread
        // 

        brpc::HttpMethod method;
        if (request->method == "GET") {
            method = brpc::HTTP_METHOD_GET; 
        } else if (request->method == "POST") {
            method = brpc::HTTP_METHOD_POST;
        } else if (request->method == "PUT") {
            method = brpc::HTTP_METHOD_PUT;
        } else if (request->method == "DELETE") {
            method = brpc::HTTP_METHOD_DELETE;
        } else {
            throw std::runtime_error("unsupported method: " + request->method);
        }
        

        brpc::Controller cntl;
        cntl.http_request().uri() = request->url;
        cntl.http_request().set_method(method);
        
        for (const auto& header : request->headers) {
            cntl.http_request().SetHeader(header.first, header.second);
        }

        for (const auto& param : request->params) {
            cntl.http_request().uri().SetQuery(param.first, param.second);
            // cntl.http_request().add_query_param(param.first, param.second);
        }

        // 设置请求体
        if (request->data.is_object()) {
            cntl.request_attachment().append(request->data.dump());
            cntl.http_request().set_content_type("application/json");
            // cntl.http_request().set_header("Content-Type", "application/json");
        }

        if (!request->stream) {
            chann_.CallMethod(nullptr, &cntl, nullptr, nullptr, nullptr);
        } else {
            cntl.response_will_be_read_progressively();
            chann_.CallMethod(nullptr, &cntl, nullptr, nullptr, nullptr);
            cntl.ReadProgressiveAttachmentBy(new StreamHttpReader(body));
        }

        response->status_code = cntl.http_response().status_code();
        for (auto it=cntl.http_response().HeaderBegin(); it != cntl.http_response().HeaderEnd(); ++it) {
            response->headers[it->first] = it->second;
        }

        // 解析响应体
        if (!cntl.response_attachment().empty()) {
            // 如果内容是 '\n\n'会不会 core?
            response->body = json::parse(cntl.response_attachment().to_string());
        }
        return Status::OK();
    }

    DECLARE_PARAMS (
        INPUT(request, InputData<HttpRequest>),
        OUTPUT(response, OutputData<HttpResponse>),
        OUTPUT(body, Stream<std::string>),
        // CONTEXT_DATA(count, int),
        // GRAPH_DEPEND(pre, PreSafety),
    );

NodeInputWrppper<InputData<HttpRequest>>& request = *BaseNode::input<InputData<HttpRequest>>("request");
NodeOutputWrppper<OutputData<HttpResponse>>& response = *BaseNode::output<OutputData<HttpResponse>>("response");
NodeOutputWrppper<Stream<std::string>>& body = *BaseNode::output<Stream<std::string>>("body"); 
std::tuple<NodeInputWrppper<InputData<HttpRequest>>&, NodeOutputWrppper<OutputData<HttpResponse>>&, NodeOutputWrppper<Stream<std::string>>&> wrappers = std::tie(request, response, body); 
using BaseNode::BaseNode; 
Status execute(BaseContext& ctx) { 
    return std::apply([this, &ctx](auto& ...args) { 
        return run(ctx.get(args)...); 
    }, wrappers); 
}
private:
    brpc::Channel chann_;

};
REGISTER_CLASS(HttpNode);

}