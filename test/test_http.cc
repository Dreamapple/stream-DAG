#include "include/stream-dag.h"
#include "include/http.h"
#include "include/sinker.h"
#include <gflags/gflags.h>

using namespace stream_dag;

class MockRequest {
public:
    std::string url;

    json to_json() {
        json j;
        j["url"] = url;
        return j;
    }
};

class MockResponse {
public:
    std::string body;

    json to_json() {
        json j;
        j["body"] = body;
        return j;
    }
};

using _SourceNode = SourceNode<MockRequest>;
using _SinkerNode = SinkerNode<MockResponse>;

REGISTER_CLASS(_SourceNode);
REGISTER_CLASS(_SinkerNode);


template<class fromT, class toT>
class ConverterNode : public BaseNode {
public:
    // ConverterNode(const std::string& name, const std::string& type, std::function<Status(fromT&, toT&)> conv)
    //      : BaseNode(name, type), conv_(conv) {}
    void set_conv(std::function<Status(fromT&, toT&)> conv) { conv_ = conv; }
    Status run(InputData<fromT>& from, OutputData<toT>& to) {
        return conv_(*from, *to);
    }
    DECLARE_PARAMS (
        INPUT(from, InputData<fromT>),
        OUTPUT(to, OutputData<toT>),
    );
private:
    std::function<Status(fromT&, toT&)> conv_;
};


template<class Request, class Response>
class Context : public BaseContext {
public:
    using BaseContext::BaseContext;
    Context(Request *request, Response *response) : request_(request), response_(response) {}
    Request *request() { return request_; }
    Response *response() { return response_; }
private:
    Request *request_ = nullptr;
    Response *response_ = nullptr;
};

int main(int argc, char* argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // 图编排，
    json option{{"timeout", 1000}};
    StreamGraph g{option};

    // source 和 sinker 应该和 BaseNode 的 Input 和 Output 等价。
    // 一个StreamGraph应该可以完美变成一个BaseNode。
    // -> 方案1 实现一个 GraphNode 然后 depend 一个 graph。手动编辑 source 和 sink
    // -> 方案2 想办法直接从底层实现等价
    _SourceNode* source = g.add_node<_SourceNode>("source");
    auto* source_convertor = g.add_node<ConverterNode<MockRequest, HttpRequest>>("source_convertor");
    HttpNode* http = g.add_node<HttpNode>("http_node");
    auto* result_convertor = g.add_node<ConverterNode<HttpResponse, MockResponse>>("result_convertor");
    _SinkerNode* sinker = g.add_node<_SinkerNode>("sinker");

    source_convertor->set_conv([](MockRequest& req, HttpRequest& http_req) {
        http_req.url = req.url;
        http_req.method = "GET";
        return Status::OK();
    });

    result_convertor->set_conv([](HttpResponse& http_resp, MockResponse& resp) {
        resp.body = http_resp.body;
        return Status::OK();
    } );

    // g.add_edge(source->src, http->request);
    // g.add_edge(http->response, sinker->sink);

    g.add_edge(source->src, source_convertor->from);
    g.add_edge(source_convertor->to, http->request);
    g.add_edge(http->response, result_convertor->from);
    g.add_edge(result_convertor->to, sinker->sink);

    g.dump("./graph.json");

    BthreadExecutor executor;

    MockRequest request{ .url = "http://www.baidu.com" };
    MockResponse response;

    Context<MockRequest, MockResponse> ctx(&request, &response);

    source->set_source(&request);
    sinker->set_sinker(&response);

    ctx.enable_trace(true);


    auto status = executor.run(g, ctx);
    if (!status.ok()) {
        printf("run err: %s\n", status.error_cstr());
        return -1;
    }

    ctx.dump("running-test_http.json");
    return 0;
}