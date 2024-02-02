#pragma once
#include "stream-dag.h"
#include <tuple>
using namespace stream_dag;


class Response {
public:
    std::string data_;
    Response() {}
    Response(const std::string& data) : data_(data) {}
    // Response(std::string data) : data_(data) {}
    json to_json() {
        return json(data_);
    }
};




class OutputNode : public BaseNode {
public:
    Status run(Stream<SafetyStatus>& presafety, Stream<ChatResponse>& llm_stream, Stream<Response>& out) {
        while (true) {
            auto [safe_data, llm_data] = when_any(presafety, llm_stream);
            if (safe_data && safe_data->status == SafetyStatus::kBlock) {
                out.append(Response("blocked!"));
                return Status::OK();
            } else if (llm_data) {
                out.append(Response(llm_data->msg));
                continue;
            } else {
                break;
            }
        }
        while (llm_stream.readable()) {
            ChatResponse rsp;
            auto status = llm_stream.read(rsp);
            out.append(Response(rsp.msg));
            continue;
        }

        if (presafety.readable()) {
            SafetyStatus result;
            auto status = presafety.read(result);
            if (result.status == SafetyStatus::kBlock) {
                out.append(Response("blocked!"));
                return Status::OK();
            }
        }

        out.append(Response("end"));

        return Status::OK();
    }

    DECLARE_PARAMS (
        INPUT(presafety, Stream<SafetyStatus>),
        INPUT(llm_stream, Stream<ChatResponse>),
        OUTPUT(out, Stream<Response>),
    );
};
REGISTER_CLASS(OutputNode);
