#pragma once
#include "butil/status.h"
#include "bthread/bthread.h"
#include "brpc_utils.h"
#include "graph.h"

namespace stream_dag {


using Status = butil::Status;

class RunningNodeInfo {
public:
    RunningNodeInfo(BaseContext& ctx_, BaseNode& node_) : ctx(ctx_), node(node_) {} 

    BaseContext& ctx;
    BaseNode& node;

    int64_t start_time=0;
    int64_t stop_time=0;
    Status status;
    BThread bthrd;

    // 为了添加节点依赖增加数据结构
    std::vector<RunningNodeInfo*> sync_prev, sync_next; // 上游、下游的同步依赖节点。方便调度
    std::function<bool(BaseContext&)> condition, action;
    std::atomic_int sync_prev_finishied_cnt{0};

    friend class BaseNode;

    void async_run() {
        start_time = butil::gettimeofday_us();
        ctx.trace_node(node.name(), node.type(), "before_execute", json());
        ctx.running_cnt++;

        bthrd = BThread([this] {
            try {
                status = node.execute(ctx);
            } catch (const std::exception& e) {
                status = Status(-1, e.what());
            }
            
            ctx.trace_node(node.name(), node.type(), "after_execute", json({{"status", status.error_code()}, {"msg", status.error_str()}}));
            
            for (auto& out: node.list_output()) {
                out->half_close(ctx.get_output(out->fullname()));
            }
            // for (auto& in_: node.list_input()) {
            //     in_->close(ctx.get_input(in_->fullname()));
            // }
            
            // if (status.ok()) 
            for (auto& next: sync_next) {
                // next->notify_one(this, status);
                next->sync_prev_finishied_cnt++;
                if (next->sync_prev_finishied_cnt.load() == next->sync_prev.size()) {
                    ctx.trace_node(node.name(), node.type(), "trigger_next", json({{"next", next->node.name()},}));
                    next->async_run();
                }
            }

            ctx.running_cnt--;
            if (ctx.running_cnt.load() == 0) {
                ctx.cond_.notify_one();
            }

            ctx.trace_node(node.name(), node.type(), "after_clean", json({{"status", status.error_code()}, {"msg", status.error_str()}}));
            stop_time = butil::gettimeofday_us();
        });
    }

    json dump() {
        json result;
        result["start_time"] = start_time;
        result["stop_time"] = stop_time;
        result["status_code"] = status.error_code();
        result["status_msg"] = status.error_str();
        return result;
    }
};

class BthreadExecutor {
public:
    BthreadExecutor() = default;
    BthreadExecutor(const std::string& name) : name_(name) {}
    Status run(BaseContext& ctx) {
        return run(*ctx.graph(), ctx);
    }

    Status run(StreamGraph& g, BaseContext& ctx) {
        std::unordered_map<std::string, RunningNodeInfo> nodes_map_;

        for (auto& node: g.list_node()) {
            node->init_ctx(ctx);
            nodes_map_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(node->name()),
                std::forward_as_tuple(ctx, *node)
            );
        }

        for (auto& dep_info: g.list_depends()) {
            RunningNodeInfo& running_node = nodes_map_.at(dep_info.node()->name());
            for (const auto& prev: dep_info.deps()) {
                RunningNodeInfo& prev_running_info = nodes_map_.at(prev->name());
                running_node.sync_prev.push_back(&prev_running_info);
                prev_running_info.sync_next.push_back(&running_node);
            }
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

        for (auto& [name, run]: nodes_map_) {
            if (run.sync_prev_finishied_cnt.load() == run.sync_prev.size()) {
                run.async_run();
            }
        }
        
        bthread::Mutex mutex_;
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        int timeout_cnt = 0;
        while (ctx.running_cnt.load() != 0) {
            int rc = ctx.cond_.wait_for(lock_, 1000000);
            if (rc == ETIMEDOUT) {
                timeout_cnt += 1;
                if (timeout_cnt >= 100) {
                    bool dumped = ctx.dump("running.json");
                    if (dumped) {
                        return Status(-1, "Timeout, dump to running.json");
                    } else {
                        printf("Executor[%s] Start dump Running Node Info:\n", name_.c_str());
                        for (auto& it: nodes_map_) {
                            json info = it.second.dump();
                            printf("Executor[%s] [%s]:%s\n", name_.c_str(), it.first.c_str(), info.dump().c_str());
                        }
                        printf("Executor[%s] ctx.running_cnt=%d\n", name_.c_str(), ctx.running_cnt.load());
                        return Status(-1, "Timeout, fail to dump");
                    }
                    break;
                }
            }
        }
        
        for (auto& [name, run]: nodes_map_) {
            int join_code = run.bthrd.join();
            if (join_code != 0) {
                printf("Internal error bthread join failed!");
                return Status(-1, "Internal error bthread join failed");
            }
        }

        return Status::OK();
    }

private:
    bool lazy_ = false;
    std::string name_;
};


}