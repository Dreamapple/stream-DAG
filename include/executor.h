#pragma once
#include "butil/status.h"
#include "bthread/bthread.h"

#include "graph.h"

namespace stream_dag {


using Status = butil::Status;

class BthreadExecutor {
public:
    Status run(StreamGraph& g, BaseContext& ctx) {
        bthread::ConditionVariable& cond_ = ctx.cond_;
        bthread::Mutex mutex_;

        for (auto& node: g.list_node()) {
            node->init_ctx(ctx);
        }

        auto& edges = g.list_edge();
        for (auto& it: edges) {
            auto out = it.first;
            auto in = it.second;
            ctx.init_input(out, in);
        }

        // for (auto& name: g.list_output_full_names()) {
        //     ctx.init_data(name);
        //     DataNode& d = ctx.get_data(name);
        //     d.on_fisrt_append([&] {
        //         for (auto& node: d.down_stream_node()) {
        //             if (!node.run()) {
        //                 bthread_start_background([&] {
        //                     node.set_running();
        //                     node.execute(ctx);
        //                 }
        //             }
        //         }
        //     });
        // }

        ctx.running_cnt.store(g.list_node().size());

        for (BaseNode* node: g.list_node()) {
            bthread_t& bid = ctx.get_bthread_id(node);

            void *args = &ctx.nodes_map_.at(node->name());

            bthread_start_background(&bid, nullptr, [](void* args) ->void* {
                RunningNodeInfo* call_args = (RunningNodeInfo*)args;
                BaseNode& node = call_args->node;
                BaseContext& ctx = call_args->ctx;
                call_args->start_time = butil::gettimeofday_us();
                Status status = node.base_execute(ctx);
                call_args->status = status;
                call_args->stop_time = butil::gettimeofday_us();

                ctx.running_cnt--;
                if (ctx.running_cnt.load() == 0) {
                    call_args->cond.notify_one();
                }

                return nullptr;
            }, args);
        }
        

        std::unique_lock<bthread::Mutex> lock_(mutex_);
        int timeout_cnt = 0;
        while (ctx.running_cnt.load() != 0) {
            int rc = cond_.wait_for(lock_, 1000000);
            if (rc == ETIMEDOUT) {
                timeout_cnt += 1;
                if (timeout_cnt >= 10) {
                    bool dumped = ctx.dump("running.json");
                    if (dumped) {
                        return Status(-1, "Timeout, dump to running.json");
                    } else {
                        for (auto it: ctx.nodes_map_) {
                            printf("dump node[%s] info:\n", it.first.c_str());
                            json info = it.second.dump();
                            printf("%s\n", info.dump().c_str());
                        }
                        return Status(-1, "Timeout, fail to dump");
                    }
                    break;
                }
            }
        }
        
        for (auto [node, bid]: ctx.get_bhtread_id_map()) {
            int join_code = bthread_join(bid, nullptr);
            if (join_code !=0) {
                printf("Internal error bthread join failed!");
                return Status(-1, "Internal error bthread join failed");
            }
        }
        return Status::OK();
    }

private:
    bool lazy_ = false;
};


}