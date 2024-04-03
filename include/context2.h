





// 节点声明的时候使用 XxxDescriptor 保存声明
// 执行的时候通过 ctx 将 XxxDescriptor 转换为 XxxHolder 持有数据
// XxxDescriptor是静态数据 只有 name 没有 ctx
// XxxHolder是执行数据 拥有上下文 ctx

template<class T, class Holder>
class Descriptor {
public:
    Descriptor(const std::string& name) : name_(name) {}
    std::string name() const { return name_; }
private:
    std::string name_;
};

class InputTag {};
class OutputTag {};
class NodeTag {};

template<class T, class Context, class Tag>
class Holder {
public:
    Holder(T& value, Context& ctx) : value_(value), ctx_(ctx) {}
    T& value() { return value_; }
    Context& ctx() { return ctx_; }
private:
    T& value_;
    Context& ctx_;
};

template<class T>
using Input = Holder<T, BaseContext, InputTag>;

template<class T>
using Output = Holder<T, BaseContext, OutputTag>;

template<class T>
using Node = Holder<T, BaseContext, NodeTag>;

template<class T>
using InputDescriptor = Descriptor<T, Input<T>>;

template<class T>
using OutputDescriptor = Descriptor<T, Output<T>>;

template<class T>
using NodeDescriptor = Descriptor<T, Node<T>>;


class Context {
public:
    Context(BaseContext& base_ctx) : base_ctx_(base_ctx) {}

    template<class T, class U>
    U get(Descriptor<T, U>& descriptor) {
        return U(get<T>(descriptor.id()), *this);
    }

    template<class T>
    T& get(int id) {
        return *(std::any_cast<T*>(&data_map_.at(id)));
    }

private:
    std::vector<std::any> data_vec_;
}
