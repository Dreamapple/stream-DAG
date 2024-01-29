
#pragma once
#include "stream-dag.h"

using namespace stream_dag;




class SafetyStatus {
public:
    int status = 0;

    static const int kBlock = -1;
    json to_json() {
        return json(status);
    }
};



class PreSafety : public BaseNode {
public:
    Status run(Stream<Start>& start, Stream<SafetyStatus>& out) {
        // time.sleep(1000)
        out.append(SafetyStatus{0});
        return Status::OK();
    }

    // 下面是生成的代码
    using BaseNode::BaseNode;
    Status execute(BaseContext& ctx) {
        auto& start = ctx.get_input<Stream<Start>>(name(), "start");
        auto& out = ctx.get_output<Stream<SafetyStatus>>(name(), "out");
        Status status = run(start, out);
        return status;
    }

    ENGINE_INPUT(start, Start);
    ENGINE_OUTPUT(out, SafetyStatus);
};

REGISTER_CLASS(PreSafety);