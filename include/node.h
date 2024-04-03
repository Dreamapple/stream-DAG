#pragma once
#include <any>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "context.h"
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
        stream->auto_close();
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

class BaseNodeWrapper {
public:
    BaseNodeWrapper(const std::string& full_name) : fullname_(full_name) {}
    ~BaseNodeWrapper() = default;
    virtual std::string fullname() { return fullname_; };

    virtual BaseNode* create(BaseContext&, const std::string& full_name) = 0 ;

private:
    std::string fullname_;
};

template<class T>
class NodeCalleeWrapper : public BaseNodeWrapper {
    using BaseNodeWrapper::BaseNodeWrapper;
    BaseNode* create(BaseContext& ctx, const std::string& data_name) {
        return new T(data_name, typeid(T).name());
    }
};

// BaseNode 的设计思想是：
//   节点有两种状态。
//   一、静态状态。是作为”图“的描述，而不是作为”执行“的逻辑
//   1. BaseNode 及子类实例化时不持有运行时的上下文 BaseContext; 只在调用 execute 接口时传入，实现更高层级的隔离。
//   2. 节点的输入输出是 BaseDataWrapper 类型，描述输入和输出，而不是实例，方便静态编排。
//   二、动态状态。是作为”执行“的逻辑，
//   1. 节点的输入输出是 BaseData 类型，描述输入和输出，而不是实例，方便动态编排。
//   2. 需要持有运行时的上下文 BaseContext。用来和ctx中的数据交换信息。
class BaseNode {
public:
    BaseNode(const std::string& name, const std::string& type) : name_(name), type_(type) {}
    virtual ~BaseNode() = default;

    // 这里初始化的是 ctx 的值  而不是BaseNode自身; TODO 把这部分代码移动到 ctx 中
    virtual Status init_ctx(BaseContext& ctx) {
        ctx.init_node(name(), this);
        for (auto& output : outputs_) {
            ctx.init_data(output->fullname(), output->create(ctx, output->fullname()));
        }

        json j = json::object();
        Status status = init(j); // TODO !!!
        // for (auto& input : inputs_) {
        //     ctx.init_data(input->fullname(), input->create(ctx, input->fullname()));
        // }
        for (auto& callee : callees_) {
            BaseNode* sub_node = callee->create(ctx, callee->fullname());
            ctx.init_node(callee->fullname(), sub_node);
            sub_node->init_ctx(ctx);
            // sub_node 是动态调用
            // 不清楚调用次数
            // 无法初始化 input output 这些
            // 只能初始化 callees_里面的内容
            // 在 call 的时候先初始化然后使用 使用后删除 是用 shared_ptr 计数 不用 ctx 存储
        }
        return Status::OK();
    }

    // 初始化节点 这个函数用来在图中使用 子类重写 init 函数
    // Status initialize(BaseContext& ctx, std::vector<std::string>& fullpath) {
    //     return Status::OK();
    // }

    virtual Status init(json& option) {
        // json& sub_option = option["sub_workers"];
        // for (auto& sub_worker_wrapper : callees_) {
        //     Status s = sub_worker->init(sub_option[sub_worker->fullname()]);
        //     if (!s.ok()) {
        //         return s;
        //     }
        // }
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

    template <class T>
    std::shared_ptr<NodeCalleeWrapper<T>> depend(const std::string& name) {
        auto wrapper = std::make_shared<NodeCalleeWrapper<T>>(name_ + "/" + name); 
        callees_.push_back(wrapper);
        return wrapper;
    }

    std::string name() const { return name_; };
    std::string type() { return type_; };
    std::vector<std::shared_ptr<BaseDataWrapper>> list_input () const { return inputs_; }
    std::vector<std::shared_ptr<BaseDataWrapper>> list_output() const { return outputs_; }
    std::vector<std::shared_ptr<BaseNodeWrapper>> list_depend() const { return callees_; }

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
        for (auto data : callees_) {
            info["callees"].push_back(data->fullname());
        }
        return info;
    }

    std::function<bool(BaseContext&)> condition_, action_;

private:
    std::string name_, type_;

    // 边依赖
    std::vector<std::shared_ptr<BaseDataWrapper>> inputs_;
    std::vector<std::shared_ptr<BaseDataWrapper>> outputs_;

    // 子模块
    std::vector<std::shared_ptr<BaseNodeWrapper>> callees_;
};


template<class RealNode>
class Node {
public:
    Node(RealNode& node, BaseContext& ctx): node_(node), ctx_(ctx) {}

    decltype(auto) Params() {
        return std::apply([this](auto&... args) {
            return std::tie(get(args)...);
        }, node_.wrappers);
    }

    Status Call() {
        return node_.execute(ctx_);
    }

    // 为了从 下面 4 行简化为 1 行 使用一点模板自动转换类型
    // auto&& [http_request, http_response, http_body] = http_node.Params(); // 1
    // http_request.append(http_req); // 2
    // Status status = http_node.Call(); // 3
    // http_response.read(http_rsp); // 4
    // 结果
    // Status status = http_node.Call(http_req, http_rsp);

    class BeforeTag {};
    class AfterTag {};
    BeforeTag before;
    AfterTag after;
    
    template<class... Args>
    Status Call(Args& ...args) {
        // ctx.init_input(out, in);
        auto tp = std::make_tuple(std::ref(args)...);
        update(tp, node_.wrappers, before); // 运行前先把数据写进去

        Status status = std::apply([this](auto&... args) {
            return node_.run(get(args)...);
        }, node_.wrappers);

        update(tp, node_.wrappers, after); // 运行后需要读出来
        return status;
    }

    template<typename... T1, typename... T2, typename Tag>
    Status update(std::tuple<T1...>& params, std::tuple<T2...>& wrappers, Tag tag) {
        static_assert(sizeof...(T1) <= sizeof...(T2)); // 参数数量可能少一点 后面的参数可能不需要
        return update(params, wrappers, std::make_index_sequence<sizeof...(T1)>{}, tag);
    }

    template<typename... T1, typename... T2, std::size_t... I, typename Tag>
    Status update(std::tuple<T1...>& t1, std::tuple<T2...>& t2, std::index_sequence<I...>, Tag tag) {
        auto s = std::tuple{ update_one(std::get<I>(t1), std::get<I>(t2), tag)... };
        return Status::OK();
    }

    template<class T>
    Status update_one(T& param, NodeInputWrppper<Stream<T>>& wrapper, BeforeTag tag) {
        // Input 类型的需要提前设置好
        auto& input = get(wrapper);
        return input.append(param);
    }

    template<class T>
    Status update_one(T& param, NodeOutputWrppper<Stream<T>>& wrapper, AfterTag tag) {
        // Output 类型的需要 结束后拿出来
        auto&  output = get(wrapper);
        return output.read(param);
    }

    template<class T1, class T2, typename Tag>
    Status update_one(T1& param, T2& wrapper, Tag tag) {
        //  其它类型先忽略
        return Status::OK();
    }

    template<class T>
    T& get(NodeInputWrppper<T>& wrapper) {
        auto it = data_map_.find(wrapper.fullname());
        if (it != data_map_.end()) {
            return *std::any_cast<std::shared_ptr<T>>(it->second);
        }
        auto ptr = std::make_shared<T>(ctx_, wrapper.fullname(), typeid(T).name());
        data_map_[wrapper.fullname()] = ptr;
        return *ptr;
    }

    template<class T>
    T& get(NodeOutputWrppper<T>& wrapper) {
        auto it = data_map_.find(wrapper.fullname());
        if (it != data_map_.end()) {
            return *std::any_cast<std::shared_ptr<T>>(it->second);
        }
        auto ptr = std::make_shared<T>(ctx_, wrapper.fullname(), typeid(T).name());
        data_map_[wrapper.fullname()] = ptr;
        return *ptr;
    }

    template<class T>
    T& get(NodeCalleeWrapper<T>& wrapper) {
        auto it = data_map_.find(wrapper.fullname());
        if (it != data_map_.end()) {
            return *std::any_cast<std::shared_ptr<T>>(it->second);
        }
        auto ptr = std::make_shared<T>(ctx_, wrapper.fullname(), typeid(T).name());
        data_map_[wrapper.fullname()] = ptr;
        return *ptr;
    }

public:
    RealNode& node_;
    BaseContext& ctx_;

    std::unordered_map<std::string, std::any> data_map_;
};

#define INPUT(name, type)  name, NodeInputWrppper<type>&, *BaseNode::input<type>(#name)
#define OUTPUT(name, type) name, NodeOutputWrppper<type>&, *BaseNode::output<type>(#name)
#define DEPEND(name, type) name, NodeCalleeWrapper<type>&, *BaseNode::depend<type>(#name)
#define GEN_RESULT(...) std::tuple<_MACRO_GET2_EVERY3_(__VA_ARGS__)> wrappers = std::tie(_MACRO_GET1_EVERY3_(__VA_ARGS__));
#define DECLARE_PARAMS(...) _MACRO_GEN_PARAMS_(__VA_ARGS__)  GEN_RESULT(__VA_ARGS__) using BaseNode::BaseNode; \
    Status execute(BaseContext& ctx) { return std::apply([this, &ctx](auto& ...args) { return run(ctx.get(args)...); }, wrappers);  }

}