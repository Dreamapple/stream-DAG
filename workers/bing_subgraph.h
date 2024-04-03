#pragma once
#include "include/stream-dag.h"
#include <brpc/channel.h>
#include <brpc/progressive_reader.h>

#include "include/http.h"


namespace stream_dag {

using json = nlohmann::json;

class BingRequest {
public:
    std::string query;
    json to_json() {
        json j;
        j["query"] = query;
        return j;
    }
};

class BingResponse {
public:
    json body;
    json to_json() {
        return body;
    }
};


class BingNode : public BaseNode {
public:
    Status init(json& option) {
        subscription_key_ = option.value("subscription_key", "a138cbf482d741fda0239d669d336693");
        return Status::OK();
    }

    // Status run(Stream<BingRequest>& bing_requests, Stream<BingResponse>& bing_responses) {
    //     return foreach(bing_request, bing_responses, 
    //         [&](BingRequest& bing_request, BingResponse& bing_response) {
    //            return run_once(bing_request, bing_response);
    //         });
    // }

    Status run(Stream<BingRequest>& bing_request_, Stream<BingResponse>& bing_response_, Node<HttpNode> http_node) {
        BingRequest req;
        bing_request_.read(req);

        HttpRequest http_req {
            .method = "GET",
            .url = "https://api.bing.microsoft.com/v7.0/search",
            .params = {
                {"q", req.query},
            },
            .headers = {
                {"Ocp-Apim-Subscription-Key", subscription_key_},
            },
        };
        HttpResponse http_rsp;

        Status status = http_node.Call(http_req, http_rsp);

        BingResponse bing_rsp {
            .body = http_rsp.body,
        };

        bing_response_.append(bing_rsp);
        return status;
    }   
    
    DECLARE_PARAMS (
        INPUT(bing_requests, Stream<BingRequest>),
        OUTPUT(bing_responses, Stream<BingResponse>),
        DEPEND(http_node, HttpNode),
    );

    std::string subscription_key_;
};
REGISTER_CLASS(BingNode);


}