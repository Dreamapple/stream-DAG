#pragma once
#include <any>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "common.h"
#include "stream.h"
#include <nlohmann/json.hpp>

namespace stream_dag_engine {

template<class T>
class SourceNode : public BaseNode {
public:
    void set_source(T* source) {
        source_ = source;
    }

    Status run(OutputData<T>& src) {
        *src = *source_;
        return Status::OK();
    }

    DECLARE_PARAMS (
        OUTPUT(src, OutputData<T>),
    );

private:
    T* source_;
};

template<class T>
class SinkerNode : public BaseNode {
public:
    void set_sinker(T* sinker) {
        sinker_ = sinker;
    }

    Status run(InputData<T>& sink) {
        printf("[ ] SinkerNode sink: %s\n", sink->to_json().dump().c_str());
        *sinker_ = *sink;
        return Status::OK();
    }

    DECLARE_PARAMS (
        INPUT(sink, InputData<T>),
    );

private:
    T* sinker_;
};


}