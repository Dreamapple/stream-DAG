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
        subscription_key_ = option.value("subscription_key", "");

        json& http_node_option = option["http_node"];
        http_node_ = new HttpNode("http_node", "HttpNode");
        http_node_->init(http_node_option);
        return Status::OK();
    }

    Status run(Stream<BingRequest>& bing_request_, Stream<BingResponse>& bing_response_) {
        BingRequest bing_req;
        bing_request_.read(bing_req);

        HttpRequest http_req {
            .method = "GET",
            .url = "https://api.bing.microsoft.com/v7.0/search",
            .params = {
                {"q", bing_req.query},
            },
            .headers = {
                {"Ocp-Apim-Subscription-Key", subscription_key_},
            },
        };
        HttpResponse http_rsp;

        BaseContext ctx;
        Stream<HttpRequest> http_req_stream(ctx, "http_req_stream", "HttpRequest");
        Stream<HttpResponse> http_rsp_stream(ctx, "http_rsp_stream", "HttpResponse");
        Stream<std::string> http_rsp_body(ctx, "http_rsp_body", "HttpResponseBody");
        http_req_stream.append(http_req);
        http_node_->run(http_req_stream, http_rsp_stream, http_rsp_body);
        http_rsp_stream.read(http_rsp);

        BingResponse bing_rsp {
            .body = http_rsp.body,
        };

        bing_response_.append(bing_rsp);
        return Status::OK();
    }
    
    DECLARE_PARAMS (
        INPUT(bing_request_, Stream<BingRequest>),
        OUTPUT(bing_response_, Stream<BingResponse>),
    );

private:
    HttpNode* http_node_ = nullptr;
    std::string subscription_key_;
};
REGISTER_CLASS(BingNode);

}