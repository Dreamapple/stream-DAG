#pragma once

#include "butil/status.h"
#include "bthread/bthread.h"
#include "bthread/butex.h"
#include "bthread/condition_variable.h"

#include <any>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <fstream>
#include <nlohmann/json.hpp>
#include "brpc_utils.h"

namespace stream_dag {
    
using json = nlohmann::json;
using Status = butil::Status;


class BaseNode;
class BaseContext;



template<class T>
class NodeInputWrppper;

template<class T>
class NodeOutputWrppper;


class BaseContext {
public:
    BaseContext() = default;
    BaseContext(const std::string& unique_id) : unique_id_(unique_id) {
        trace_buf_["unique_id"] = unique_id;
    }
    virtual ~BaseContext() = default;

    template <class T>
    void init_data(const std::string& name, T&& value) {
        output_map_[name] = value;
        trace_buf_["streams"][name] = json::array();
    }

    void init_node(const std::string& name, BaseNode* node) {
        trace_buf_["nodes"][name] = json::array();
    }

    void init_data(const std::string& name, std::any&& value) {
        output_map_.emplace(name, value);
        trace_buf_["streams"][name] = json::array();
    }

    void init_input(const std::string& out, const std::string& in) {
        input_map_[in] = out;
    }

    // 避免节点 A 的输入 a 和节点 B 的输入 a 混淆，需要把节点名称也加上
    // 需要规范 name，只能 `[_A-Za-z0-9]+`
    template <class T>
    T& get_input(const std::string& node_name, const std::string& name) {
        std::string fullname = node_name + "/" + name;
        return get_input<T>(fullname);
    }

    template <class T>
    T& get_output(const std::string& node_name, const std::string& name) {
        std::string fullname = node_name + "/" + name;
        return get_output<T>(fullname);
    }

    template <class T>
    T& get_input(const std::string& name) {
        std::string output_name = input_map_.at(name);
        return get_output<T>(output_name);
    }

    template <class T>
    T& get_output(const std::string& name) {
        std::any & val = output_map_.at(name);
        std::shared_ptr<T> out_val = std::any_cast<std::shared_ptr<T>>(val);
        return *out_val;
    }

    std::any& get_output(const std::string& name) {
        return output_map_.at(name);
    }

    std::any& get_input(const std::string& name) {
        std::string output_name = input_map_.at(name);
        return get_output(output_name);
    }

    template <class T>
    T& get(NodeInputWrppper<T>& wrapper) {
        return get_input<T>(wrapper.fullname());
    }

    template <class T>
    T& get(NodeOutputWrppper<T>& wrapper) {
        return get_output<T>(wrapper.fullname());
    }

    void enable_trace(bool enable=true) {
        enable_trace_ = enable;
    }

    void trace_node(const std::string& name, const std::string& type, const std::string& event, json data) {
        if (enable_trace_) {
            return trace("nodes", name, type, event, data);
        }
    }

    void trace_stream(const std::string& name, const std::string& type, const std::string& event, json data) {
        if (enable_trace_) {
            return trace("streams", name, type, event, data);
        }
    }

    void trace(const std::string& report_type, const std::string& name, const std::string& type, const std::string& event, json data) {
        json report;
        report["report_type"] = report_type;
        report["name"] = name;
        report["type"] = type;
        report["event"] = event;
        report["time"] = butil::gettimeofday_us();
        report["data"] = data;
        trace_buf_[report_type][name].push_back(report);
        // printf("[#] %s\n", report.dump(4).c_str());
    }

    bool dump(const std::string& path) {
        if (enable_trace_) {
            std::ofstream ofs(path);
            ofs << trace_buf_.dump(4);
            ofs.close();
            return true;
        }
        return false;
    }

    // for executor
    std::atomic_int running_cnt{0};

    // for notify
    bthread::ConditionVariable cond_;

    

private:
    std::unordered_map<std::string, std::string> input_map_;
    std::unordered_map<std::string, std::any> output_map_;

    std::unordered_map<BaseNode*, bthread_t> bthread_id_map_;

    

    // for trace
    std::string unique_id_;
    json trace_buf_;
    bool enable_trace_ = false;
};

}
