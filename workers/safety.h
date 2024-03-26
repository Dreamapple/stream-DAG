
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
    
    DECLARE_PARAMS(
        INPUT(start, Stream<Start>),
        OUTPUT(out, Stream<SafetyStatus>)
    )
};

REGISTER_CLASS(PreSafety);
