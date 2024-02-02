#pragma once
#include "stream-dag.h"

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

    INPUT(presafety, Stream<SafetyStatus>);
    INPUT(llm_stream, Stream<ChatResponse>);
    OUTPUT(out, Stream<Response>);

    // 下面是生成的代码
    using BaseNode::BaseNode;
    Status execute(BaseContext& ctx) {
        // auto& presafety_ = ctx.get_input(presafety);
        auto& presafety_ = ctx.get(OutputNode::presafety);
        auto& llm_stream_ = ctx.get(OutputNode::llm_stream);
        auto& out_ = ctx.get(OutputNode::out);
        Status status = run(presafety_, llm_stream_, out_);
        return status;
    }
};
REGISTER_CLASS(OutputNode);
