#pragma once



#include <map>
#include <string>
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeinfo>
#include "node.h"

namespace stream_dag {


// 工厂类
template <class BaseT, class... Args>
class Factory {
public:
    template<class T>
    static std::unique_ptr<BaseT> CreateInstance(Args&&... args) {
        return CreateInstanceByName(typeid(T).name(), std::forward<Args>(args)...);
    }
    template<typename T>
    static void Register() {
        const char* typeName = typeid(T).name();
        RegisterByName<T>(typeName);
    }

    static std::unique_ptr<BaseT> CreateInstanceByName(const std::string& typeName, Args&&... args) {
        auto it = GetRegistry().find(typeName);
        if (it != GetRegistry().end())
            return it->second(std::forward<Args>(args)...);
        return nullptr;
    }

    static std::vector<std::string> GetRegisteredTypes() {
        std::vector<std::string> types;
        for (const auto& pair : GetRegistry()) {
            types.push_back(pair.first);
        }
        return types;
    }

    template<typename T>
    static void RegisterByName(const std::string& typeName) {
        auto it = GetRegistry().find(typeName);
        if (it != GetRegistry().end())
            throw std::runtime_error(std::string("Type already registered: ") + typeName);

        GetRegistry()[typeName] = [](Args&&... args) -> std::unique_ptr<BaseT> {
            return std::make_unique<T>(std::forward<Args>(args)...);
        };
    }

private:
    static std::map<std::string, std::function<std::unique_ptr<BaseNode>(Args&&... args)>>& GetRegistry() {
        static std::map<std::string, std::function<std::unique_ptr<BaseNode>(Args&&... args)>> registry;
        return registry;
    }
};

using NodeFactory = Factory<BaseNode, const std::string&, const std::string&>;

/*
Factory::RegisterByName<T, __VA_ARGS__>(#T);

*/

// 定义宏来简化类的注册过程
#define REGISTER_CLASS_WITH_ARGS(T, ...) \
    class T##Initializer { \
    public: \
        T##Initializer() { \
            Factory<BaseNode, __VA_ARGS__>::Register<T>(); \
        } \
    }; \
    T##Initializer _##T##InitializerInstance;

#define REGISTER_CLASS(T) \
    class T##Initializer { \
    public: \
        T##Initializer() { \
            NodeFactory::Register<T>(); \
        } \
    }; \
    T##Initializer _##T##InitializerInstance;


// 使用宏注册类
class Derived : public BaseNode {
public:
    using BaseNode::BaseNode;
};

// REGISTER_CLASS(Derived);


// // 主函数
// int main() {
//     std::unique_ptr<BaseNode> instance = NodeFactory::CreateInstanceByName("Derived", "Derived_name", "Derived");
//     // ...
//     return 0;
// }
#define CONCAT(A, B) _CONCAT(A, B)
#define _CONCAT(A, B) A##B
#define MacroArgCount(...) _MacroArgCount(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define _MacroArgCount(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, COUNT, ...) COUNT
#define _MACRO_GET1_EVERY3_(...) CONCAT(_MACRO_GET1_EVERY3_, MacroArgCount(__VA_ARGS__))(__VA_ARGS__)
#define _MACRO_GET1_EVERY3_0() 
#define _MACRO_GET1_EVERY3_1(_1) 
#define _MACRO_GET1_EVERY3_2(_1, _2) 
#define _MACRO_GET1_EVERY3_3(_1, _2, _3) _1
#define _MACRO_GET1_EVERY3_4(_1, _2, _3, _4) _1
#define _MACRO_GET1_EVERY3_5(_1, _2, _3, _4, _5) _1
#define _MACRO_GET1_EVERY3_6(_1, _2, _3, _4, _5, _6) _1, _4
#define _MACRO_GET1_EVERY3_7(_1, _2, _3, _4, _5, _6, _7) _1, _4
#define _MACRO_GET1_EVERY3_8(_1, _2, _3, _4, _5, _6, _7, _8) _1, _4
#define _MACRO_GET1_EVERY3_9(_1, _2, _3, _4, _5, _6, _7, _8, _9) _1, _4, _7
#define _MACRO_GET1_EVERY3_10(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10) _1, _4, _7
#define _MACRO_GET1_EVERY3_11(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11) _1, _4, _7
#define _MACRO_GET1_EVERY3_12(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12) _1, _4, _7, _10
#define _MACRO_GET1_EVERY3_13(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13) _1, _4, _7, _10
#define _MACRO_GET1_EVERY3_14(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14) _1, _4, _7, _10
#define _MACRO_GET1_EVERY3_15(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15) _1, _4, _7, _10, _13
#define _MACRO_GET1_EVERY3_16(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16) _1, _4, _7, _10, _13
#define _MACRO_GET1_EVERY3_17(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17) _1, _4, _7, _10, _13
#define _MACRO_GET1_EVERY3_18(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18) _1, _4, _7, _10, _13, _16
#define _MACRO_GET1_EVERY3_19(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19) _1, _4, _7, _10, _13, _16
#define _MACRO_GET1_EVERY3_20(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20) _1, _4, _7, _10, _13, _16
#define _MACRO_GET1_EVERY3_21(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21) _1, _4, _7, _10, _13, _16, _19
#define _MACRO_GET1_EVERY3_22(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22) _1, _4, _7, _10, _13, _16, _19
#define _MACRO_GET1_EVERY3_23(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23) _1, _4, _7, _10, _13, _16, _19
#define _MACRO_GET1_EVERY3_24(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24) _1, _4, _7, _10, _13, _16, _19, _22
#define _MACRO_GET1_EVERY3_25(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25) _1, _4, _7, _10, _13, _16, _19, _22
#define _MACRO_GET1_EVERY3_26(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26) _1, _4, _7, _10, _13, _16, _19, _22
#define _MACRO_GET1_EVERY3_27(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27) _1, _4, _7, _10, _13, _16, _19, _22, _25
#define _MACRO_GET1_EVERY3_28(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28) _1, _4, _7, _10, _13, _16, _19, _22, _25
#define _MACRO_GET1_EVERY3_29(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29) _1, _4, _7, _10, _13, _16, _19, _22, _25
#define _MACRO_GET1_EVERY3_30(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30) _1, _4, _7, _10, _13, _16, _19, _22, _25, _28
#define _MACRO_GET2_EVERY3_(...) CONCAT(_MACRO_GET2_EVERY3_, MacroArgCount(__VA_ARGS__))(__VA_ARGS__)
#define _MACRO_GET2_EVERY3_0() 
#define _MACRO_GET2_EVERY3_1(_1) 
#define _MACRO_GET2_EVERY3_2(_1, _2) 
#define _MACRO_GET2_EVERY3_3(_1, _2, _3) _2
#define _MACRO_GET2_EVERY3_4(_1, _2, _3, _4) _2
#define _MACRO_GET2_EVERY3_5(_1, _2, _3, _4, _5) _2
#define _MACRO_GET2_EVERY3_6(_1, _2, _3, _4, _5, _6) _2, _5
#define _MACRO_GET2_EVERY3_7(_1, _2, _3, _4, _5, _6, _7) _2, _5
#define _MACRO_GET2_EVERY3_8(_1, _2, _3, _4, _5, _6, _7, _8) _2, _5
#define _MACRO_GET2_EVERY3_9(_1, _2, _3, _4, _5, _6, _7, _8, _9) _2, _5, _8
#define _MACRO_GET2_EVERY3_10(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10) _2, _5, _8
#define _MACRO_GET2_EVERY3_11(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11) _2, _5, _8
#define _MACRO_GET2_EVERY3_12(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12) _2, _5, _8, _11
#define _MACRO_GET2_EVERY3_13(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13) _2, _5, _8, _11
#define _MACRO_GET2_EVERY3_14(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14) _2, _5, _8, _11
#define _MACRO_GET2_EVERY3_15(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15) _2, _5, _8, _11, _14
#define _MACRO_GET2_EVERY3_16(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16) _2, _5, _8, _11, _14
#define _MACRO_GET2_EVERY3_17(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17) _2, _5, _8, _11, _14
#define _MACRO_GET2_EVERY3_18(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18) _2, _5, _8, _11, _14, _17
#define _MACRO_GET2_EVERY3_19(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19) _2, _5, _8, _11, _14, _17
#define _MACRO_GET2_EVERY3_20(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20) _2, _5, _8, _11, _14, _17
#define _MACRO_GET2_EVERY3_21(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21) _2, _5, _8, _11, _14, _17, _20
#define _MACRO_GET2_EVERY3_22(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22) _2, _5, _8, _11, _14, _17, _20
#define _MACRO_GET2_EVERY3_23(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23) _2, _5, _8, _11, _14, _17, _20
#define _MACRO_GET2_EVERY3_24(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24) _2, _5, _8, _11, _14, _17, _20, _23
#define _MACRO_GET2_EVERY3_25(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25) _2, _5, _8, _11, _14, _17, _20, _23
#define _MACRO_GET2_EVERY3_26(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26) _2, _5, _8, _11, _14, _17, _20, _23
#define _MACRO_GET2_EVERY3_27(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27) _2, _5, _8, _11, _14, _17, _20, _23, _26
#define _MACRO_GET2_EVERY3_28(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28) _2, _5, _8, _11, _14, _17, _20, _23, _26
#define _MACRO_GET2_EVERY3_29(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29) _2, _5, _8, _11, _14, _17, _20, _23, _26
#define _MACRO_GET2_EVERY3_30(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30) _2, _5, _8, _11, _14, _17, _20, _23, _26, _29
#define _MACRO_GET3_EVERY3_(...) CONCAT(_MACRO_GET3_EVERY3_, MacroArgCount(__VA_ARGS__))(__VA_ARGS__)
#define _MACRO_GET3_EVERY3_0() 
#define _MACRO_GET3_EVERY3_1(_1) 
#define _MACRO_GET3_EVERY3_2(_1, _2) 
#define _MACRO_GET3_EVERY3_3(_1, _2, _3) _3
#define _MACRO_GET3_EVERY3_4(_1, _2, _3, _4) _3
#define _MACRO_GET3_EVERY3_5(_1, _2, _3, _4, _5) _3
#define _MACRO_GET3_EVERY3_6(_1, _2, _3, _4, _5, _6) _3, _6
#define _MACRO_GET3_EVERY3_7(_1, _2, _3, _4, _5, _6, _7) _3, _6
#define _MACRO_GET3_EVERY3_8(_1, _2, _3, _4, _5, _6, _7, _8) _3, _6
#define _MACRO_GET3_EVERY3_9(_1, _2, _3, _4, _5, _6, _7, _8, _9) _3, _6, _9
#define _MACRO_GET3_EVERY3_10(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10) _3, _6, _9
#define _MACRO_GET3_EVERY3_11(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11) _3, _6, _9
#define _MACRO_GET3_EVERY3_12(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12) _3, _6, _9, _12
#define _MACRO_GET3_EVERY3_13(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13) _3, _6, _9, _12
#define _MACRO_GET3_EVERY3_14(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14) _3, _6, _9, _12
#define _MACRO_GET3_EVERY3_15(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15) _3, _6, _9, _12, _15
#define _MACRO_GET3_EVERY3_16(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16) _3, _6, _9, _12, _15
#define _MACRO_GET3_EVERY3_17(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17) _3, _6, _9, _12, _15
#define _MACRO_GET3_EVERY3_18(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18) _3, _6, _9, _12, _15, _18
#define _MACRO_GET3_EVERY3_19(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19) _3, _6, _9, _12, _15, _18
#define _MACRO_GET3_EVERY3_20(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20) _3, _6, _9, _12, _15, _18
#define _MACRO_GET3_EVERY3_21(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21) _3, _6, _9, _12, _15, _18, _21
#define _MACRO_GET3_EVERY3_22(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22) _3, _6, _9, _12, _15, _18, _21
#define _MACRO_GET3_EVERY3_23(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23) _3, _6, _9, _12, _15, _18, _21
#define _MACRO_GET3_EVERY3_24(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24) _3, _6, _9, _12, _15, _18, _21, _24
#define _MACRO_GET3_EVERY3_25(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25) _3, _6, _9, _12, _15, _18, _21, _24
#define _MACRO_GET3_EVERY3_26(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26) _3, _6, _9, _12, _15, _18, _21, _24
#define _MACRO_GET3_EVERY3_27(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27) _3, _6, _9, _12, _15, _18, _21, _24, _27
#define _MACRO_GET3_EVERY3_28(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28) _3, _6, _9, _12, _15, _18, _21, _24, _27
#define _MACRO_GET3_EVERY3_29(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29) _3, _6, _9, _12, _15, _18, _21, _24, _27
#define _MACRO_GET3_EVERY3_30(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30) _3, _6, _9, _12, _15, _18, _21, _24, _27, _30
#define _MACRO_GEN_PARAMS_(...) CONCAT(_MACRO_GEN_PARAMS_, MacroArgCount(__VA_ARGS__))(__VA_ARGS__)
#define _MACRO_GEN_PARAMS_0() 
#define _MACRO_GEN_PARAMS_1(_1) 
#define _MACRO_GEN_PARAMS_2(_1, _2) 
#define _MACRO_GEN_PARAMS_3(_1, _2, _3) _2 _1 = _3;
#define _MACRO_GEN_PARAMS_4(_1, _2, _3, _4) _2 _1 = _3;
#define _MACRO_GEN_PARAMS_5(_1, _2, _3, _4, _5) _2 _1 = _3;
#define _MACRO_GEN_PARAMS_6(_1, _2, _3, _4, _5, _6) _2 _1 = _3;_5 _4 = _6;
#define _MACRO_GEN_PARAMS_7(_1, _2, _3, _4, _5, _6, _7) _2 _1 = _3;_5 _4 = _6;
#define _MACRO_GEN_PARAMS_8(_1, _2, _3, _4, _5, _6, _7, _8) _2 _1 = _3;_5 _4 = _6;
#define _MACRO_GEN_PARAMS_9(_1, _2, _3, _4, _5, _6, _7, _8, _9) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;
#define _MACRO_GEN_PARAMS_10(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;
#define _MACRO_GEN_PARAMS_11(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;
#define _MACRO_GEN_PARAMS_12(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;
#define _MACRO_GEN_PARAMS_13(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;
#define _MACRO_GEN_PARAMS_14(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;
#define _MACRO_GEN_PARAMS_15(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;
#define _MACRO_GEN_PARAMS_16(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;
#define _MACRO_GEN_PARAMS_17(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;
#define _MACRO_GEN_PARAMS_18(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;_17 _16 = _18;
#define _MACRO_GEN_PARAMS_19(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;_17 _16 = _18;
#define _MACRO_GEN_PARAMS_20(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;_17 _16 = _18;
#define _MACRO_GEN_PARAMS_21(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;_17 _16 = _18;_20 _19 = _21;
#define _MACRO_GEN_PARAMS_22(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;_17 _16 = _18;_20 _19 = _21;
#define _MACRO_GEN_PARAMS_23(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;_17 _16 = _18;_20 _19 = _21;
#define _MACRO_GEN_PARAMS_24(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;_17 _16 = _18;_20 _19 = _21;_23 _22 = _24;
#define _MACRO_GEN_PARAMS_25(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;_17 _16 = _18;_20 _19 = _21;_23 _22 = _24;
#define _MACRO_GEN_PARAMS_26(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;_17 _16 = _18;_20 _19 = _21;_23 _22 = _24;
#define _MACRO_GEN_PARAMS_27(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;_17 _16 = _18;_20 _19 = _21;_23 _22 = _24;_26 _25 = _27;
#define _MACRO_GEN_PARAMS_28(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;_17 _16 = _18;_20 _19 = _21;_23 _22 = _24;_26 _25 = _27;
#define _MACRO_GEN_PARAMS_29(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;_17 _16 = _18;_20 _19 = _21;_23 _22 = _24;_26 _25 = _27;
#define _MACRO_GEN_PARAMS_30(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30) _2 _1 = _3;_5 _4 = _6;_8 _7 = _9;_11 _10 = _12;_14 _13 = _15;_17 _16 = _18;_20 _19 = _21;_23 _22 = _24;_26 _25 = _27;_29 _28 = _30;

}