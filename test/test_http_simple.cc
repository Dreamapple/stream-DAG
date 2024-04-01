#include "include/stream-dag.h"
#include "include/http.h"
// #include "include/sinker.h"
#include <gflags/gflags.h>

using namespace stream_dag;

class SourceNode : public BaseNode {
public:
    Status run(Stream<HttpRequest>& src) {
        HttpRequest req { 
            .method = "GET",
            .url = "https://api.bing.microsoft.com/v7.0/search", 
            .params = {
                {"q", "Microsoft"},
                // {"mkt", "zh-CN"},
                // {"count", "10"},
            },
            .headers = {
                {"Ocp-Apim-Subscription-Key", "a138cbf482d741fda0239d669d336693"},
                {"Content-Type", "application/x-www-form-urlencoded"},
            },
        };
        src.append(req);
        return Status::OK();
    }

    DECLARE_PARAMS (
        OUTPUT(src, Stream<HttpRequest>),
    );
};
REGISTER_CLASS(SourceNode);

// class SourceNodeInitializer { public: SourceNodeInitializer() { NodeFactory::Register<SourceNode>(); } }; 

// SourceNodeInitializer _SourceNodeInitializerInstance;

class SinkerNode : public BaseNode {
public:
    Status run(Stream<HttpResponse>& result) {
        
        HttpResponse res;
        result.read(res);

        return Status::OK();
    }

    DECLARE_PARAMS (
        INPUT(result, Stream<HttpResponse>),
    );
};


REGISTER_CLASS(SinkerNode);


int main(int argc, char* argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // 图编排，
    json option;
    option["host"] = "https://api.bing.microsoft.com";

    StreamGraph g{option};

    SourceNode* source = g.add_node<SourceNode>("source");
    HttpNode* http = g.add_node<HttpNode>("http_node");
    SinkerNode* sinker = g.add_node<SinkerNode>("sinker");

    http->init(option);
// bing_search.search(query, result)
    // 

    g.add_edge(source->src, http->request_);
    g.add_edge(http->response_, sinker->result);

    g.dump("./test_http2_graph.json");

    BthreadExecutor executor;

    BaseContext ctx;
    ctx.enable_trace(true);


    auto status = executor.run(g, ctx);
    if (!status.ok()) {
        printf("run err: %s\n", status.error_cstr());
        return -1;
    }

    ctx.dump("running-test_http.json");
    return 0;
}