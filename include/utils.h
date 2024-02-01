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




}