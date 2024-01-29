
#pragma once
#include "node.h"
#include "utils.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <nlohmann/json.hpp>

namespace stream_dag {

using json = nlohmann::json;

class StreamGraph {
public:
    StreamGraph() = default;

    void add_node(BaseNode& node) {
        nodes_.push_back(&node);
    }

    template <class T>
    T* add_node(const std::string& name) {
        T* node = new T(name, typeid(T).name());
        nodes_.push_back(node);
        return node;
    }

    BaseNode* add_node(const std::string& name, const std::string& type) {
        std::unique_ptr<BaseNode> node = NodeFactory::CreateInstanceByName(type, name, type);
        BaseNode* ptr = node.release();
        nodes_.push_back(ptr);
        return ptr;
    }

    template <class T1, class T2>
    void add_edge(std::shared_ptr<T1> out, std::shared_ptr<T2> in) {
        static_assert(std::is_same<T1, T2>::value, "type must be same");
        edge_[out->fullname()] = in->fullname();
    }

    void add_edge(const std::string& out, const std::string& in) {
        edge_[out] = in;
    }

    std::vector<BaseNode*> list_node() {
        return nodes_;
    }

    const std::unordered_map<std::string, std::string>& list_edge() {
        return edge_;
    }

    std::vector<std::string> list_output_full_names() {
        std::vector<std::string> output_data;
        for (auto node : nodes_) {
            // for (auto data : node->list_output()) {
            //     output_data.push_back(data->name());
            // }
        }
        return output_data;
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
        return Status::OK();
    }

    void dump(const std::string& path) {
        json result;
        json& nodes = result["nodes"];
        json& edges = result["edges"];

        int index = 0;
        for (auto node : nodes_) {
            json& node_info = nodes[index]; //  = node.to_json();
            node_info["name"] = node->name();
            node_info["type"] = node->type();
            for (auto data : node->list_input()) {
                node_info["inputs"].push_back(data->fullname());
            };
            for (auto data : node->list_output()) {
                node_info["outputs"].push_back(data->fullname());
            }
            index += 1;
        }

        index = 0;
        for (auto edge : edge_) {
            edges[index]["from"] = edge.first;
            edges[index]["to"] = edge.second;
            index += 1;
        }

        std::ofstream out(path, std::ofstream::out);
        out << result;
        out.close();
        return;
    }

private:
    std::vector<BaseNode*> nodes_;
    std::unordered_map<std::string, std::string> edge_;
};



}