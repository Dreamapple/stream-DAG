
#pragma once
#include "node.h"
#include "factory.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <nlohmann/json.hpp>

namespace stream_dag {

using json = nlohmann::json;


enum class DependentType {
    Unknown = 0,
    SYNC = 1,
    CONDITIONAL = 2,
};

class Calculator {
public:
    Calculator() = default;
    Calculator(std::string& code) : code_(code) {}
    template<class Return>
    Return eval(BaseContext& ctx) {
        return Return();
    }
private:
    std::string code_;
};

class Condition : public Calculator {
public:
    Condition() = default;
    Condition(std::string code) : Calculator(code), code_(code) {}
    Condition(std::function<bool(BaseContext&)> callback) : callback_(callback) {}
    Condition(std::string code, std::function<bool(BaseContext&)> callback) : Calculator(code), code_(code), callback_(callback) {}
    bool check(BaseContext& ctx) {
        if (callback_) {
            return callback_(ctx);
        } else {
            return Calculator::eval<bool>(ctx);
        }
    }

    json to_json() const {
        return code_;
    }
private:
    std::string code_;
    std::function<bool(BaseContext&)> callback_;
};

#define CONDITION(expr) Condition(#expr, [] (BaseContext&) { return expr; })

class DependentInfo {
public:
    DependentInfo() = default;
    DependentInfo(BaseNode* node, std::vector<BaseNode*>& deps, Condition condition) 
     : node_(node), deps_(deps), condition_(condition) {}
    DependentInfo(BaseNode* node, std::vector<BaseNode*>& deps, DependentType type, Condition condition) 
     : node_(node), deps_(deps), type_(type), condition_(condition) {}

    BaseNode* node() const { return node_; }
    const DependentType type() const { return type_; }
    const std::vector<BaseNode*> deps() const { return deps_; }

    json to_json() const{
        json result;
        result["node"] = node_->name();
        // result["dependent_type"] = (int)type_;
        result["condition"] = condition_.to_json();
        json& dependent_nodes = result["dependent"] = json::array();

        for (auto node : deps_) {
            dependent_nodes.push_back(node->name());
        }
        return result;
    }

private:
    BaseNode* node_;
    std::vector<BaseNode*> deps_;
    DependentType type_;
    Condition condition_;
};

class StreamGraph {
public:
    StreamGraph() = default;
    ~StreamGraph() {
        for (auto node : nodes_) {
            delete node;
        }
    }

    // void add_node(BaseNode& node) {
    //     nodes_.push_back(&node);
    //     nodes_map_[node.name()] = &node;
    // }

    template <class T>
    T* add_node(const std::string& name) {
        T* node = new T(name, typeid(T).name());
        nodes_.push_back(node);
        nodes_map_[name] = node;
        return node;
    }

    BaseNode* add_node(const std::string& name, const std::string& type) {
        std::unique_ptr<BaseNode> node = NodeFactory::CreateInstanceByName(type, name, type);
        BaseNode* ptr = node.release();
        nodes_.push_back(ptr);
        nodes_map_[name] = ptr;
        return ptr;
    }

    void add_node_dep(BaseNode* node, std::vector<BaseNode*> deps, Condition&& condition) {
        depends_.push_back(DependentInfo{node, deps, condition});
    }

    void add_node_dep(const std::string& name, std::vector<std::string>& deps_name, const std::string& condition_string) {
        BaseNode* node = nodes_map_.at(name);
        std::vector<BaseNode*> deps;
        for (auto& dep_name: deps_name) {
            BaseNode* dep_node = nodes_map_.at(dep_name);
            deps.push_back(dep_node);
        }
        
        depends_.push_back({node, deps, condition_string});
    }

    // template <class Context>
    // void add_node_dep(BaseNode* node, std::vector<BaseNode*> deps, Condition&& condition) {
    //     StreamGraph::add_node_dep(node, deps, [node, condition] (BaseContext& ctx) -> bool {
    //         Context& context = dynamic_cast<Context&>(ctx);
    //         return condition(context);
    //     });
    // }

    template <class T1, class T2>
    void add_edge(std::shared_ptr<T1> out, std::shared_ptr<T2> in) {
        static_assert(std::is_same<T1, T2>::value, "type must be same");
        edge_[out->fullname()] = in->fullname();
    }

    template <class T1, class T2>
    void add_edge(NodeOutputWrppper<T1>& out, NodeInputWrppper<T2>& in) {
        static_assert(std::is_same<T1, T2>::value, "type must be same");
        edge_[out.fullname()] = in.fullname();
    }

    template <class T1, class T2>
    void add_edge_dep(NodeInputWrppper<T2>& in, NodeOutputWrppper<T1>& out) {
        static_assert(std::is_same<T1, T2>::value, "type must be same");
        edge_[out.fullname()] = in.fullname();
    }

    void add_edge(const std::string& out, const std::string& in) {
        edge_[out] = in;
    }

    std::vector<BaseNode*> list_node() {
        return nodes_;
    }

    const std::vector<DependentInfo>& list_depends() const { return depends_; }

    const std::unordered_map<std::string, std::string>& list_edge() {
        return edge_;
    }

    Status load(const std::string& path) {
        json graph;
        std::ifstream in(path);
        if (!in.is_open()) {
            return Status(-1, "open file failed");
        }
        in >> graph;
        in.close();

        for (auto& node : graph["nodes"]) {
            std::string type = node["type"];
            std::string name = node["name"];
            add_node(name, type);
        }
        for (auto& edge : graph["edges"]) {
            add_edge(edge["from"], edge["to"]);
        }

        for (auto& depend: graph["depents"]) {
            std::string node_name = depend["node"];
            std::string condition = depend["condition"];
            std::vector<std::string> dependent = depend["dependent"];
            add_node_dep(node_name, dependent, condition);
        }
        return Status::OK();
    }

    void dump(const std::string& path) {
        json result;
        json& nodes = result["nodes"];
        json& edges = result["edges"];

        for (int64_t index=0; index<nodes_.size(); index++) {
            BaseNode* node = nodes_[index];
            nodes[index] = node->to_json();
        }

        for (auto edge : edge_) {
            edges.push_back({
                {"from", edge.first},
                {"to", edge.second}
            });
        }

        for (auto& dep: depends_) {
            result["depends"].push_back(dep.to_json());
        }

        std::ofstream out(path, std::ofstream::out);
        out << result;
        out.close();
        return;
    }

    friend class BaseNode;
private:
    std::vector<BaseNode*> nodes_;
    std::unordered_map<std::string, std::string> edge_;

    // 节点 map 
    std::unordered_map<std::string, BaseNode*> nodes_map_;

    // 节点依赖
    std::vector<DependentInfo> depends_;
};



}