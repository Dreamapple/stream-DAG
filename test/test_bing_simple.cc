#include "include/stream-dag.h"
#include "workers/bingnode.h"
// #include "include/sinker.h"
#include <gflags/gflags.h>

using namespace stream_dag;

class SourceNode : public BaseNode {
public:
    Status run(Stream<BingRequest>& src) {
        BingRequest req { 
            .query = "hello+world",
        };
        src.append(req);
        return Status::OK();
    }

    DECLARE_PARAMS (
        OUTPUT(src, Stream<BingRequest>),
    );
};
REGISTER_CLASS(SourceNode);


class SinkerNode : public BaseNode {
public:
    Status run(Stream<BingResponse>& result) {
        
        BingResponse res;
        result.read(res);

        return Status::OK();
    }

    DECLARE_PARAMS (
        INPUT(result, Stream<BingResponse>),
    );
};


REGISTER_CLASS(SinkerNode);


int main(int argc, char* argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // 图编排，
    json httpoption;
    httpoption["host"] = "https://api.bing.microsoft.com";

    json option;
    option["http_node"] = httpoption;
    option["subscription_key"] = "a138cbf482d741fda0239d669d336693";

    StreamGraph g{option};

    SourceNode* source = g.add_node<SourceNode>("source");
    BingNode* bing = g.add_node<BingNode>("bing_node");
    SinkerNode* sinker = g.add_node<SinkerNode>("sinker");

    bing->init(option);
// 
    // 

    g.add_edge(source->src, bing->bing_request_);
    g.add_edge(bing->bing_response_, sinker->result);

    g.dump("./test_bing_graph.json");

    BthreadExecutor executor;

    BaseContext ctx;
    ctx.enable_trace(true);


    auto status = executor.run(g, ctx);
    if (!status.ok()) {
        printf("run err: %s\n", status.error_cstr());
        return -1;
    }

    ctx.dump("running-bing_http.json");
    return 0;
}