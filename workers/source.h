#pragma once
#include "stream-dag.h"

using namespace stream_dag;

struct Start {
    std::string msg;
    json to_json() {
        return json(msg);
    }
};


class Source : public BaseNode {
public:
    Status run(Stream<Start>& start) {
        // time.sleep(1000)
        start.append(Start{});
        return Status::OK();
    }

    DECLARE_PARAMS(
        OUTPUT(input, Stream<Start>)
    )
};

REGISTER_CLASS(Source);