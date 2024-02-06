#pragma once
#include <any>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "common.h"
#include "stream.h"
#include <nlohmann/json.hpp>

namespace stream_dag {

class BaseContext;
class BaseDataWrapper {
public:
    BaseDataWrapper(const std::string& node_name, const std::string& data_name) : fullname_(node_name + "/" + data_name) {}
    ~BaseDataWrapper() = default;
    virtual std::string fullname() { return fullname_; };

    virtual std::any create(BaseContext&, const std::string& data_name) = 0 ;
    virtual void half_close(std::any &data) = 0 ;
private:
    std::string fullname_;
};

template<class T>
class DataWrppper : public BaseDataWrapper {
public:
    using data_type = T;
    using BaseDataWrapper::BaseDataWrapper;

    std::any create(BaseContext& ctx, const std::string& data_name) {
        return std::make_shared<T>(ctx, data_name, typeid(T).name());
    }

    void half_close(std::any &data) {
        auto stream = std::any_cast<std::shared_ptr<T>>(data);
        stream->half_close();
    }
};

template<class T>
class NodeInputWrppper : public DataWrppper<T> {
    using data_type = T;
    using DataWrppper<T>::DataWrppper;
};

template<class T>
class NodeOutputWrppper : public DataWrppper<T> {
    using data_type = T;
    using DataWrppper<T>::DataWrppper;
};


class BaseNode {
public:
    BaseNode(const std::string& name, const std::string& type) : name_(name), type_(type) {}
    virtual ~BaseNode() = default;

    // 这里初始化的是 ctx 的值  而不是BaseNode自身
    virtual Status init_ctx(BaseContext& ctx) {
        ctx.init_node(name(), this);
        for (auto& output : outputs_) {
            ctx.init_data(output->fullname(), output->create(ctx, output->fullname()));
        }
        return Status::OK();
    }

    virtual Status execute(BaseContext& ctx) = 0;

    template<class ...T> Status run(T ...inouts);
    
    template <class T>
    std::shared_ptr<NodeInputWrppper<T>> input(const std::string& name) {
        auto wrapper = std::make_shared<NodeInputWrppper<T>>(name_, name); 
        inputs_.push_back(wrapper);
        return wrapper;
    }

    template <class T>
    std::shared_ptr<NodeOutputWrppper<T>> output(const std::string& name) {
        auto wrapper = std::make_shared<NodeOutputWrppper<T>>(name_, name); 
        outputs_.push_back(wrapper);
        return wrapper;
    }

    std::string name() { return name_; };
    std::string type() { return type_; };
    std::vector<std::shared_ptr<BaseDataWrapper>> list_input() const { return inputs_; }
    std::vector<std::shared_ptr<BaseDataWrapper>> list_output() const { return outputs_; }

    json to_json() {
        json info;
        info["name"] = name_;
        info["type"] = type_;
        for (auto data : inputs_) {
            info["inputs"].push_back(data->fullname());
        };
        for (auto data : outputs_) {
            info["outputs"].push_back(data->fullname());
        }


        return info;
    }

    std::function<bool(BaseContext&)> condition_, action_;

private:
    std::string name_, type_;

    // 边依赖
    std::vector<std::shared_ptr<BaseDataWrapper>> inputs_;
    std::vector<std::shared_ptr<BaseDataWrapper>> outputs_;

};


#define INPUT(name, type) name, NodeInputWrppper<type>&, *BaseNode::input<type>(#name)
#define OUTPUT(name, type) name, NodeOutputWrppper<type>&, *BaseNode::output<type>(#name)
#define GEN_RESULT(...) std::tuple<_MACRO_GET2_EVERY3_(__VA_ARGS__)> wrappers = std::tie(_MACRO_GET1_EVERY3_(__VA_ARGS__));
#define DECLARE_PARAMS(...) _MACRO_GEN_PARAMS_(__VA_ARGS__)  GEN_RESULT(__VA_ARGS__) using BaseNode::BaseNode; \
    Status execute(BaseContext& ctx) { return std::apply([this, &ctx](auto& ...args) { return run(ctx.get(args)...); }, wrappers);  }

}