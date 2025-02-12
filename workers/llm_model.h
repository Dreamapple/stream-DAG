#pragma once
#include "stream-dag.h"

using namespace stream_dag;



class ChatRequest {
public:
    std::string msg;
    json to_json() {
        return json(msg);
    }
};


class ChatResponse {
public:
    ChatResponse() = default;
    ChatResponse(std::string msg_):msg(msg_) {}
    std::string msg;
    json to_json() {
        return json(msg);
    }
};


class LLMModel : public BaseNode {
public:
    Status run(Stream<ChatRequest>& reqs, Stream<ChatResponse>& rsps) {
        // 业务代码
        auto [status, req] = reqs.read();
        rsps.append(ChatResponse("测试数据 1"));
        rsps.append(ChatResponse("测试数据 2"));
        rsps.append(ChatResponse("测试数据 3"));
        rsps.append(ChatResponse("测试数据 4"));
        rsps.append(ChatResponse("测试数据 5"));
        rsps.append(ChatResponse("测试数据 6"));
        return Status::OK();
    }
    
    DECLARE_PARAMS( INPUT(req, Stream<ChatRequest>), OUTPUT(rsp, Stream<ChatResponse>))
};
REGISTER_CLASS(LLMModel);