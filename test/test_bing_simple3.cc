#include "include/stream-dag.h"
#include "workers/bing_subgraph.h"
// #include "include/sinker.h"
#include <gflags/gflags.h>

using namespace stream_dag;

class TestNode : public BaseNode {
public:
    DECLARE_PARAMS (
        DEPEND(bing_node, BingNode),
    ) {
        BingRequest req { 
            .query = "hello+world",
        };
        BingResponse rsp;
        Status s = bing_node.Call(req, rsp);
        printf("%s\n", rsp.body.dump().c_str());
        return Status::OK();
    }
};
REGISTER_CLASS(TestNode);

class Context : public BaseContext {
public:
    Context(BingRequest& req, BingResponse& rsp) {
        graph_ = new Graph();

    }
};

int main(int argc, char* argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    StreamGraph g;

    TestNode* source = g.add_node<TestNode>("test");
    
    g.dump("./test_bing_graph3.json");

    BthreadExecutor executor;

    BaseContext ctx(&g, "test");
    ctx.enable_trace(true);


    auto status = executor.run(ctx);
    if (!status.ok()) {
        printf("run err: %s\n", status.error_cstr());
        return -1;
    }

    ctx.dump("running-bing2_http.json");
    return 0;
}