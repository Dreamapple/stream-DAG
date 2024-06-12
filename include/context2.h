





// 节点声明的时候使用 XxxDescriptor 保存声明
// 执行的时候通过 ctx 将 XxxDescriptor 转换为 XxxHolder 持有数据
// XxxDescriptor是静态数据 只有 name 没有 ctx
// XxxHolder是执行数据 拥有上下文 ctx


class InputTag {};
class OutputTag {};
class NodeTag {};

template<class T, class Tag, class SaveT=T::SaveT>
class Holder : public T::HolderT {
public:
    Holder(const std::string& name) : name_(name) {}
    T& operator*() {
        if (value == nullptr) {
            value = ctx().get<SaveT, Tag>(name_);
        }
        return *T;
    }
    T* operator->() {
        if (value == nullptr) {
            value = ctx().get<SaveT, Tag>(name_);
        }
        return value;
    }

private:
    std::string name_;
    T* value_;
};

template<class T>
using Input = Holder<T, InputTag>;

template<class T>
using Output = Holder<T, OutputTag>;

template<class T>
using Node = Holder<T, NodeTag>;


class Context {
public:
    Context() {
        // CHECK_EQ(0, bthread_key_create(&key_, my_thread_local_data_deleter));
    }
    ~Context() {
        // bthread_key_delete(key_);
    }

    template<class T>
    U get(Descriptor<T, U>& descriptor) {
        return U(get<T>(descriptor.id()), *this);
    }

    template<class T>
    T& get(int id) {
        return *(std::any_cast<T*>(&data_map_.at(id)));
    }

    Context make(int slot) {
        return Context{this, slot};
    }

private:
    bthread_key_t _tls2_key;
    std::vector<std::any> data_vec_;
}


class BaseWorker {
public:
    virtual Status run(BaseContext& ctx) = 0;
    virtual ~BaseWorker() = default;

    template<class T>
    Input<T> input(const std::string& name) {
        slot += 1;
        return Input<T>(name);
    }

    template<class T>
    Output<T> output(const std::string& name) {
        slot += 1;
        return Output<T>(name);
    }

    template<class T>
    Node<T> depend(const std::string& name) {
        slot += 1;
        return Node<T>(name);
    }

private:
    int slot = 0;
};


class CallWorker : public BaseWorker {
public:
    Status search(SearchRequest& req, SearchResponse& rsp) {
        return Status::OK();
    }
    // EXPOSE(search);

    struct HolderT {
        using search = CallProxy<CallWorker::search>;
    };
}


template<class ReqT, class RspT, class DepT>
class Worker : public BaseWorker {
public:
    Input<ReqT> req = input<ReqT>("req"); // 动态创建
    Output<RspT> resp = output<RspT>("rsp"); // 动态创建
    Node<DepT> dep = depend<DepT>("dep"); // 不存在时创建 放到 ctx 中

    Status Init() {

    }

    Status Execute(Context& ctx) {
        Context sub_ctx{ctx, slot};
        bthread_set_specific(ContextKey(), &sub_ctx);
        Status s = Run();
        bthread_set_specific(ContextKey(), nullptr); // del 
        // sub_ctx 析构
        return s;
    }

    virtual Status Run() {
        ReqT req_data = *req;
        RspT rsp_data = *rsp;
        CALL(dep, req_data, rsp_data);
        dep.search();
        CALL(dep, search, )
        cost("name");
        dep.search()
        req->half_close();
    }
};

void cost(const std::string& name) {
    Context& ctx = *(Context*)bthread_get_specific(ContextKey());
    ctx().cost(name);
}






