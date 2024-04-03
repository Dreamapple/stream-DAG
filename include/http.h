#pragma once
#include "stream-dag.h"
#include <brpc/channel.h>
#include <brpc/progressive_reader.h>

namespace stream_dag {

using json = nlohmann::json;

struct HttpRequest {
    std::string method = "GET";
    std::string url;
    std::map<std::string, std::string> params;
    std::map<std::string, std::string> headers;
    json data;

    bool stream = false; // 流式读取数据
    int timeout_ms = 0; // 超时时间，单位ms

    json to_json() {
        json j;
        j["method"] = method;
        j["url"] = url;
        j["params"] = params;
        j["headers"] = headers;
        j["data"] = data;
        j["stream"] = stream;
        j["timeout_ms"] = timeout_ms;
        return j;
    }
};

class HttpResponse {
public:
    int status_code;
    std::map<std::string, std::string> headers;
    json body;

    json to_json() {
        json j;
        j["status_code"] = status_code;
        j["headers"] = headers;
        j["body"] = body;
        return j;
    }
};

class StreamHttpReader : public brpc::ProgressiveReader {
public:
    StreamHttpReader(Stream<std::string>& body) : body_(body) {
        body.set_auto_close(false);
    }

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
        std::string host = option.value("host", "https://api.bing.microsoft.com");
        std::string lb = option.value("lb", "");
        int timeout_ms = option.value("timeout_ms", 20000);
        int max_retry = option.value("max_retry", 3);

        brpc::ChannelOptions opt;
        opt.protocol = brpc::PROTOCOL_HTTP;
        if (timeout_ms > 0) { opt.timeout_ms = timeout_ms; }
        if (max_retry > 0) { opt.max_retry = max_retry; }
        return (0 == chann_.Init(host.c_str(), lb.c_str(), &opt)) ? Status::OK() : Status(-1, "init channnel failed");
    }

    Status run(Stream<HttpRequest>& request_, Stream<HttpResponse>& response_, Stream<std::string>& stream_body) {
        // 这里面没有必要写异步任务
        // 直接同步执行, 阻塞时 brpc 会自动调度到其它bthread
        // 

        HttpRequest requestdata, *request;
        request_.read(requestdata);

        request = &requestdata;

        brpc::HttpMethod method = brpc::HTTP_METHOD_GET;
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
            cntl.ReadProgressiveAttachmentBy(new StreamHttpReader(stream_body));
        }

        HttpResponse responsedata, *response;
        response = &responsedata;

        response->status_code = cntl.http_response().status_code();
        for (auto it=cntl.http_response().HeaderBegin(); it != cntl.http_response().HeaderEnd(); ++it) {
            response->headers[it->first] = it->second;
        }

        // 解析响应体
        if (!cntl.response_attachment().empty()) {
            // 如果内容是 '\n\n'会不会 core?
            response->body = json::parse(cntl.response_attachment().to_string());
        }
        response_.append(responsedata);
        return Status::OK();
    }

    DECLARE_PARAMS (
        INPUT(request_, Stream<HttpRequest>),
        OUTPUT(response_, Stream<HttpResponse>),
        OUTPUT(stream_body, Stream<std::string>),
    );

private:
    brpc::Channel chann_;

};
REGISTER_CLASS(HttpNode);

}
