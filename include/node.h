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
        return std::make_shared<Stream<T>>(ctx, data_name, typeid(T).name());
    }

    void half_close(std::any &data) {
        auto stream = std::any_cast<std::shared_ptr<Stream<T>>>(data);
        stream->half_close();
    }
};

class BaseNode {
public:
    BaseNode(const std::string& name, const std::string& type) : name_(name), type_(type) {}
    virtual ~BaseNode() = default;

    // 这里初始化的是 ctx 的值  而不是BaseNode自身
    virtual Status init_ctx(BaseContext& ctx) {
        ctx.init_node(name(), this);
        for (auto& output : output_) {
            ctx.init_data(output->fullname(), output->create(ctx, output->fullname()));
        }
        return Status::OK();
    }

    Status base_execute(BaseContext& ctx) {
        ctx.trace_node(name(), type(), "before_execute", json());
        Status status = execute(ctx);
        ctx.trace_node(name(), type(), "after_execute", json({{"status", status.error_code()}, {"msg", status.error_str()}}));

        for (auto& out: list_output()) {
            out->half_close(ctx.get_output(out->fullname()));
        }
        return status;
    }

    virtual Status execute(BaseContext& ctx) = 0;

    template<class ...T> Status run(T ...inouts);
    
    template <class T>
    std::shared_ptr<DataWrppper<T>> input(const std::string& name) {
        auto wrapper = std::make_shared<DataWrppper<T>>(name_, name); 
        inputs_.push_back(wrapper);
        return wrapper;
    }

    template <class T>
    std::shared_ptr<DataWrppper<T>> output(const std::string& name) {
        auto wrapper = std::make_shared<DataWrppper<T>>(name_, name); 
        output_.push_back(wrapper);
        return wrapper;
    }

    std::string name() { return name_; };
    std::string type() { return type_; };
    std::vector<std::shared_ptr<BaseDataWrapper>> list_input() { return inputs_; }
    std::vector<std::shared_ptr<BaseDataWrapper>> list_output() { return output_; }

private:
    std::string name_, type_;

    std::vector<std::shared_ptr<BaseDataWrapper>> inputs_;
    std::vector<std::shared_ptr<BaseDataWrapper>> output_;
};

// #define ENGINE_INPUT(name, type) std::shared_ptr<BaseDataWrapper> name = BaseNode::input<type>(#name);
// #define ENGINE_OUTPUT(name, type) std::shared_ptr<BaseDataWrapper> name = BaseNode::output<type>(#name);
#define ENGINE_INPUT(name, type) std::shared_ptr<DataWrppper<type>> name = BaseNode::input<type>(#name);
#define ENGINE_OUTPUT(name, type) std::shared_ptr<DataWrppper<type>> name = BaseNode::output<type>(#name);


}