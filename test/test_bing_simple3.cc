#include "include/stream-dag.h"
#include "workers/bing_subgraph.h"
// #include "include/sinker.h"
#include <gflags/gflags.h>

using namespace stream_dag;

class TestNode : public BaseNode {
public:
    Status run(Node<BingNode> bing_node) {
        BingRequest req { 
            .query = "hello+world",
        };
        BingResponse rsp;
        Status s = bing_node.Call(req, rsp);
        printf("%s\n", rsp.body.dump().c_str());
        return Status::OK();
    }

    DECLARE_PARAMS (
        DEPEND(bing_node, BingNode),
    );
};
REGISTER_CLASS(TestNode);


int main(int argc, char* argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    StreamGraph g;

    TestNode* source = g.add_node<TestNode>("test");
    
    g.dump("./test_bing_graph3.json");

    BthreadExecutor executor;

    BaseContext ctx;
    ctx.enable_trace(true);


    auto status = executor.run(g, ctx);
    if (!status.ok()) {
        printf("run err: %s\n", status.error_cstr());
        return -1;
    }

    ctx.dump("running-bing2_http.json");
    return 0;
}