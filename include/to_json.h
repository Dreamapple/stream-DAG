#pragma once
#include <nlohmann/json.hpp>


namespace stream_dag {

using json = nlohmann::json;

json to_json(const std::string& str) {
    return json(str);
}

template<class T>
json to_json(const std::vector<T>& vec) {
    json j;
    for (auto& v : vec) {
        j.push_back(v);
    }
    return j;
}


// 声明一个辅助类型，用于检测成员函数 to_json 的存在
template<typename T, typename = void>
struct has_to_json : std::false_type {};

// 特化，如果 T 有一个名为 to_json 的成员函数，则继承 std::true_type
template<typename T>
struct has_to_json<T, decltype((void) &T::to_json, void())> : std::true_type {};

// 基础模板，如果 T 没有 to_json 方法，直接序列化
template<class T, typename std::enable_if<!has_to_json<T>::value, int>::type = 0>
json to_json(const T& value) {
    return json(value);
}

// 模板特化，如果 T 有 to_json 方法，则调用该方法
template<class T, typename std::enable_if<has_to_json<T>::value, int>::type = 0>
json to_json(const T& value) {
    return value.to_json();
}


}