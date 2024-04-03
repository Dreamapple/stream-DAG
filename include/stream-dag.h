#pragma once
#include "context.h"
#include "stream.h"
#include "node.h"
#include "executor.h"
#include "graph.h"
#include "factory.h"
#include "when_any.h"

namespace stream_dag {

class StreamDAG {
public:
    StreamDAG() = default;
    ~StreamDAG() = default;

    Status init();
    Status execute(BaseContext& ctx);

private:
    std::vector<BaseNode*> nodes;
};


}