// 一个实现了 trace 功能的 worker最小化实现
// 依赖注入是为了 ctx 的传递

#include "butil/status.h"
#include "bthread/bthread.h"
#include "bthread/butex.h"
#include "bthread/condition_variable.h"

#include <nlohmann/json.hpp>

#include <deque>
#include <fstream>
#include <assert.h>

using json = nlohmann::json;
using butil::Status;


// 自动就是 detach 状态，可以直接析构
class BThread {
public:
    BThread() = default;

    template< class Function, class... Args >
    explicit BThread( Function&& fn, Args&&... args ) {
        auto p_wrap_fn = new auto([=]{ fn(args...);   });
        auto call_back = [](void* ar) ->void* {
            auto f = reinterpret_cast<decltype(p_wrap_fn)>(ar);
            (*f)();
            delete f;
            return nullptr;
        };
        bthread_start_background(&bthid_, nullptr, call_back, (void*)p_wrap_fn);
    }

    int join() {
        if (bthid_ != INVALID_BTHREAD) {
            return bthread_join(bthid_, NULL);
        }
        return -1;
    }

    bthread_t get_tid() {
        return bthid_;
    }

private:
    bthread_t bthid_ = INVALID_BTHREAD;
};

// BThread Wrapper end

// 在业务系统中，主要还是需要 trace 调用关系和参数的传递关系，还有这个ctx 的参数在哪里使用了这种问题。
// 1. trace_call. call 的时候 py 中不会创建新的 ctx；go 中可能创建，进而传递。似乎应该创建 trace_ctx; Node 不创建。子调用在 trace_ctx 中创建。
//    A call B 调用 N 次，C -> A -> B ; D -> A -> B 这种调用链其实是不同的，trace_ctx 可能会比较好处理。
//    如果有静态图，节点、边是比较明确的。但是动态图中，节点是一个trace_ctx，一个 Node<Worker>可能被调用很多次，出现多次。时序图中应该还是一个节点。
// 2. trace_args. 在 trace_ctx 中track 到参数。
// 3. trace_data context access
class TraceImpl {
public:
    TraceImpl() = default;
    TraceImpl(const std::string& name) : name_(name) {
        trace_buf_["name"] = name;
    }
    TraceImpl(const json& buf) : trace_buf_(buf) { }
    virtual ~TraceImpl() {}

    void set_name(const std::string name) { 
        name_ = name; 
        trace_buf_["name"] = name;
    }
    const std::string& name() const { return name_; }

    template<class Target>
    void trace(Target* pointer, const std::string& event, const json& value) {
        printf("[#] [%s] %p %s %s \n", name_.c_str(), pointer, event.c_str(), value.dump().c_str());

        u_int64_t address = reinterpret_cast<u_int64_t>(pointer);

        json report;
        report["type"] = typeid(Target).name();
        report["address"] = address;
        report["event"] = event;
        report["time"] = butil::gettimeofday_us();
        report["data"] = value;
        trace_buf_["trace"].push_back(report);
    }

    bool dump(const std::string& path) {
        std::ofstream ofs(path);
        ofs << collect_trace_info().dump(4);
        ofs.close();
        return true;
    }

    json collect_trace_info() {
        json result = trace_buf_;
        for (std::shared_ptr<TraceImpl> child: children_) {
            result["children"].push_back(child->collect_trace_info());
        }
        for (std::shared_ptr<TraceImpl> child: args_) {
            result["args"].push_back(child->collect_trace_info());
        }
        for (std::shared_ptr<TraceImpl> child: call_) {
            result["call"].push_back(child->collect_trace_info());
        }
        return result;
    }

    void track(std::shared_ptr<TraceImpl> child) {
        children_.push_back(child);
    }

    void track_args(std::shared_ptr<TraceImpl> child) {
        args_.push_back(child);
    }

    void track_call(std::shared_ptr<TraceImpl> child) {
        call_.push_back(child);
    }

protected:
    std::string name_;
    json trace_buf_ = json::object();

    std::vector<std::shared_ptr<TraceImpl>> children_, args_, call_;
};

class Tracable {
public:
    Tracable() : pimpl(std::make_shared<TraceImpl>()) {}
    Tracable(const std::string& name) : pimpl(std::make_shared<TraceImpl>(name)) {}
    Tracable(const json& buf) : pimpl(std::make_shared<TraceImpl>(buf)) {}
    Tracable(Tracable&& other) {
        pimpl = other.pimpl;
        other.pimpl = nullptr;
    }
    virtual ~Tracable() { }
    template<class Target>
    void trace(Target* pointer, const std::string& event, const json& value) {
        pimpl->trace(pointer, event, value);
    }

    bool dump(const std::string& path) {
        return pimpl->dump(path);
    }

    void track_args(Tracable* child) {
        pimpl->track_args(child->pimpl);
    }

    void track_call(Tracable* child) {
        pimpl->track_call(child->pimpl);
    }

    void track(Tracable* child) {
        pimpl->track(child->pimpl);
    }

    void set_trace_object(Tracable* obj) {
        pimpl = obj->pimpl;
    }

    void set_name(const std::string name) { pimpl->set_name(name); }
    const std::string& name() const { return pimpl->name(); }
protected:
    std::shared_ptr<TraceImpl> pimpl;
};



class Context : public Tracable {
public:
    Context() = default;
    Context(Context* parent, const json& buf) : Tracable(buf), parent_(parent) {
        if (parent) parent->track_call(this);
    }

    // 创建一个worker worker 由 AppContext 初始化和 trace
    // 调用时，直接使用已经持有的指针
    // 主要是为了实现依赖注入
    template<class T>
    T* CreateWorker(const std::string& name) {
        T* ptr = new T;
        ptr->set_name(name);
        return ptr;
    }

protected:
    Context* parent_ = nullptr;
};


// ctx 绑定 bthread
bthread_key_t& ContextKey() {
    static bthread_key_t key = []() {
        bthread_key_t k;
        bthread_key_create(&k, nullptr);
        return k;
    }();
    return key;
}

Context& context() {
    return *(Context*)bthread_getspecific(ContextKey());
}

void push(Context& ctx_) {
    bthread_setspecific(ContextKey(), &ctx_);
}

void pop(Context& ctx_) {
    assert(&context() == &ctx_);
    bthread_setspecific(ContextKey(), nullptr);
}
// ctx 绑定 bthread end

template<class T>
class Input;
template<class T>
class Output;
template<class T>
class Stream;

template<class T>
class NameHelper : public Tracable {
public:
    NameHelper(T& value) : value_(value) {}

    void track_args() {
        context().track_args(this);
    }

    operator Input<T>() {
        Input<T> val(value_);
        val.set_trace_object(this);
        return std::move(val);
    }

    operator Output<T>() {
        Output<T> val(value_);
        val.set_trace_object(this);
        return std::move(val);
    }
private:
    std::string name_;
    T& value_;
    Context* ctx_ = nullptr;
};

template<class T>
class NameHelper<Stream<T>> {
public:
    NameHelper(Stream<T>& value) : value_(value) {}
    void set_name(const std::string name) {
        value_.set_name(name); 
    }

    void track_args() {
        context().track_args(&value_);
    }

    operator Stream<T>&() {
        return value_;
    }

private:
    Stream<T>& value_;
};

template<class T>
NameHelper<T> wrap_arg(T& t) {
    return NameHelper<T>(t);
}

// 输入、输出、Node 等描述符定义

// 设置名称的函数模板
template<size_t... Is, typename... Args>
void set_names_helper(std::tuple<Args...> & t, std::index_sequence<Is...>) {
    ((std::get<Is>(t).set_name("arg_#" + std::to_string(Is + 1))), ...);
}

// 对 tuple 中的每个元素调用相应的方法
template<typename... Args>
void set_names(std::tuple<Args...> & t) {
    set_names_helper(t, std::make_index_sequence<sizeof...(Args)>());
}

// 设置名称的函数模板
template<size_t... Is, typename... Args>
void track_args_helper(std::tuple<Args...> & t, std::index_sequence<Is...>) {
    ((std::get<Is>(t).track_args()), ...);
}

// 对 tuple 中的每个元素调用相应的方法
template<typename... Args>
void track_args(std::tuple<Args...> & t) {
    track_args_helper(t, std::make_index_sequence<sizeof...(Args)>());
}

template<class T>
class Node {
public:
    Node(const std::string& name) : name_(name) { 
        value_ = context().CreateWorker<T>(name);
    }

    T& operator*() {
        return *value_;
    }

    T* operator->() {
        return value_;
    }

    // 使用 stream 的时候，需要异步执行任务
    // BThread对 args 使用了等值捕获`[=] { ... } ` 
    // 但是 Stream 是不可以复制的，只能引用。这样设计是为了保证 Stream 在多协程下的数据安全。
    // 所以这里需要直接引用捕获到闭包。
    // TODO 这里需要有一个机制保证 Stream 析构是在这个函数运行结束后。
    template<class ...Args>
    Status async_invoke(Args &&...args) {
        Context& ctx_ = context();

        BThread b([&ctx_, this, &args...] () {
            Context new_ctx(&ctx_, {
                {"name", name_},
                {"type", typeid(this).name()},
            });
            push(new_ctx);

            auto full_args = std::make_tuple(wrap_arg(args)...);
            set_names(full_args);

            track_args(full_args);
            
            // json trace_info {
            //     {"from", ctx_.name()},
            //     {"call", new_ctx.name()},
            // };
            // new_ctx.trace(this, "CALL", trace_info);
            Status s = std::apply([this](auto& ...args) { 
                return value_->Run(args...); 
            }, full_args);
            // Status s = value_->Run(std::forward<Args>(args)...);
            pop(new_ctx);
        });

        return Status::OK();
    }

private:
    std::string name_;
    T* value_;
    Context* ctx_ = nullptr;
};

template<class T>
class Input : public Tracable {
public:
    Input(T& value) : value_(&value) {
        json trace_val {
            {"type", typeid(T).name() },
            {"value", value},
        };
        trace(this, "CREATE", trace_val);
    }
    void set_name(const std::string& name) { 
        trace(this, "set_name", json{{"set_name", name }});
        name_ = name; 
    }
    const std::string& name() const { return name_; }
    T& operator*() {
        trace(this, "DEREF", *value_);
        return *value_;
    }

    T* operator->() {
        json val(*value_);
        trace(this, "DEREF", *value_);
        return value_;
    }
private:
    std::string name_;
    T* value_;
    Context* ctx_ = nullptr;
};

template<class T>
class Output : public Tracable {
public:
    Output(T& value) : value_(&value) {
        json trace_val {
            {"type", typeid(T).name() },
            {"value", value},
        };
        trace(this, "CREATE", trace_val);
    }
    void set_name(const std::string name) { 
        trace(this, "set_name", json{{"set_name", name }});
        name_ = name; 
    }
    const std::string& name() const { return name_; }
    T& operator*() {
        trace(this, "DEREF", *value_);
        return *value_;
    }

    T* operator->() {
        trace(this, "DEREF", *value_);
        return value_;
    }
private:
    std::string name_;
    T* value_;
    Context* ctx_ = nullptr;
};

// 输入、输出、Node 等描述符定义 end

// Stream 类型定义

template<class T>
class Stream : public Tracable{
public:
    Stream() = default;
    Stream(const std::string& name, const std::string& type) : name_(name), type_(type) {}
    virtual ~Stream() = default;

    void close(Status status=Status::OK()) {
        std::unique_lock<bthread::Mutex> lock_(mutex_);

        json trace_info;
        trace_info["code"] = status.error_code();
        trace_info["msg"] = status.error_str();
        trace("Stream::close", trace_info);

        closed_ = true;

        // lock_.unlock();
        cond_.notify_one();
    }

    bool closed() {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        return closed_;
    }

    Status push(const T& value) {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        trace("Stream::push", value);
        buf_.push_back(value);
        cond_.notify_one();
        return Status::OK();
    }

    Status pop(T& value) {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        while (buf_.empty() && !closed_) {
            int rc = cond_.wait_for(lock_, 1000000); // wait 1s
            if (rc == ETIMEDOUT) {
                trace("Stream::read wait timeout, continue wait", json());
                continue;
            }
            if (rc != 0) {
                return Status(-1, "Stream::read wait %s", berror(rc));
            }
            trace("PipeStreamBase::read wake", json({{"rc", rc}}));
        }

        if (closed_ || buf_.empty()) {
            return Status(1, "Stream::read closed");
        }

        value = buf_.front();
        buf_.pop_front();
        return Status::OK();
    }

protected:
    void trace(const std::string& event, json value) {
        // 直接路由到 worker 的执行事件视角；而不是stream的视角
        // name 是强需要的字段；但是初始化的时候不能要求用户传递
        // 一种方案是要求用户通过宏创建，创建的时候同时获取名称；但是在自动打包的时候无法生效
        // 另一种是 call 的时候再按照顺序设置名字编码，但是自动打包的时候怎么设置编码可能比较复杂
        // 但是同时支持自动装箱和名称注入是否会冲突？先固定不支持自动装箱，简洁一点。
        // 后面需要的话，得实现一个 NameWrapper，实现自动转换类型，感觉比较麻烦，报错可能很多。
        // trace(event, /* target */ name, value);
        // trace_buf_.append({
        //     {"event", event},
        //     {"value", value}
        // });
        Tracable::trace(this, event, value);
    }

    std::string name_, type_;

    std::deque<T> buf_;
    bool closed_ = false;

    bthread::ConditionVariable cond_;
    bthread::Mutex mutex_;

    Context* ctx_ = nullptr;

public:

    class iterator {
    private:
        Stream<T> *stream_ = nullptr;
        T storage_;

        void increment_() {
            Status s = stream_->pop(storage_);
            if (!s.ok()) {
                stream_ = nullptr;
            }
        }

    public:
        iterator() = default;

        explicit iterator( Stream<T> *stream) : stream_(stream) { increment_(); }
        iterator( iterator const& other) noexcept : stream_{ other.chan_ } { }

        iterator& operator=(iterator const& other) noexcept {
            if ( this != & other) {
                stream_ = other.stream_;
            }
            return * this;
        }

        bool operator==( iterator const& other) const noexcept {
            return other.stream_ == stream_;
        }

        bool operator!=( iterator const& other) const noexcept {
            return other.stream_ != stream_;
        }

        iterator & operator++() {
            increment_();
            return *this;
        }

        const iterator operator++( int) = delete;

        T& operator*() noexcept {
            return storage_;
        }

        T* operator->() noexcept {
            return &storage_;
        }
    };

    friend class iterator;
};

template<class T>
typename Stream<T>::iterator begin(Stream<T>& stream) {
    return typename Stream<T>::iterator(&stream);
}

template<class T>
typename Stream<T>::iterator end(Stream<T>& stream) {
    return typename Stream<T>::iterator();
}

// Stream 类型定义 end


class BaseWorker {
public:
    void set_name(const std::string name) { name_ = name; }
    const std::string& name() const { return name_; }

    template<class T>
    Node<T> depend(const std::string& name) {
        return Node<T>(name);
    }

    virtual ~BaseWorker() = default;
    virtual Status Init(json& option) = 0;

    // Run 中的参数都是引用?
    // Stream 必须是引用
    // 非 Stream 应该是可以按照普通常数传递, 直接复制等
    // virtual Status Run();

protected:
    std::string name_;
};

class LLMChatWorker : public BaseWorker {
public:
    // using BaseWorker::BaseWorker; 避免每次需要 using，删除这个接口
    Status Init(json& option) {
        return Status::OK();
    }
    Status Run(Stream<json>& req, Input<int> cnt,  Stream<json>& rsp) {
        json val;
        req.pop(val);
        printf("LLMChatWorker req %s %d\n", val.dump().c_str(), *cnt);
        rsp.push("name");
        rsp.push(*cnt);
        rsp.close();
        return Status::OK();
    }
};

class ChatRequest {
public:
    std::string query;
};
class ChatResponse {
public:
    std::string msg;
};
class AgentState {};

void to_json(json& j, const ChatRequest& p)
{
    j = json{ {"query", p.query} };
}

void to_json(json& j, const ChatResponse& p)
{
    j = json{ {"msg", p.msg} };
}

class ChatAgentNew : public BaseWorker {
public:
    // using BaseWorker::BaseWorker; 避免每次需要 using，删除这个接口
    Node<LLMChatWorker> llm_chat_worker = depend<LLMChatWorker>("llm_chat_worker");
    // Input<ChatRequest> chat_request = input<ChatRequest>("chat_request");
    // Output<ChatResponse> chat_response = output<ChatResponse>("chat_response");

    // Depend(llm_chat_worker, LLMChatWorker); 依赖的节点 依赖注入 从配置文件获取配置直接初始化 不需要配置
    // 初始化的时机是什么时候？
    // 应该是程序运行的时候载入配置文件就初始化
    // 使用的是 AppContext

    // 参数不需要名称 只需要知道是参数即可 这些参数本来就是临时栈上分配的 为了最小化实现 先不管理了
    // Input(chat_request, ChatRequest); 输入输出 主要是标记名称 要不然就是匿名的输入和输出了
    // Output(chat_response, ChatResponse); 需要注意输入和输出的顺序需要和函数的声明顺序一致

    Status Init(json& option) {
        llm_chat_worker->Init(option["llm_chat_worker"]);
        return Status::OK();
    }

    // Stream当然是不能拷贝的，只能传递引用
    // 但是 Node 是一个持有 ctx 中对象的 Holder 完全可以随意拷贝
    // Input 到底更像 Node 还是 Stream？
    // 如果支持类型转换 是否可以转换为 Stream？
    Status Run(Input<ChatRequest> chat_request, Stream<ChatResponse>& chat_response) {
        AgentState state;
        int cnt = 0;

        while (cnt++ < 10) {
            Stream<json> req, rsp;
            req.push(chat_request->query);

            // go 语言中会直接丢弃返回值
            // 直接使用 C++的 std::future 会有互斥问题。
            // 如果自己实现会需要考虑很多边界。有需要再添加
            llm_chat_worker(req, cnt, rsp);
            ASYNC(llm_chat_worker, Run, req, cnt, rsp);
            llm_chat_worker.async_invoke(req, cnt, rsp);

            for (auto& item: rsp) {
                // auto tool_name = item["tool_name"];
                // auto context = item["context"];
                printf("item: %s\n", item.dump().c_str());
                ChatResponse r{
                    .msg = item.dump(),
                };
                chat_response.push(r);
            }

        }
        chat_response.close();

        // Stream<json> s1, s2, s3;
        // llm_chat_worker(s1, s2);
        // llm_chat_worker(s1, s3);
        // s1.push("hello");

        return Status::OK();
    }
};

int main(int argc, char const *argv[])
{
    Context ctx(nullptr, {{"name", "RootContext"}});
    push(ctx);

    ChatRequest chat_request {
        .query = "chat_query... kdsds",
    };
    Stream<ChatResponse> chat_response;
    Node<ChatAgentNew> agent("agent");
    auto fut = agent(chat_request, chat_response);

    for (auto& rsp: chat_response) {
        printf("chat_response msg: %s\n", rsp.msg.c_str());
    }

    ctx.dump("/home/linghui/stream-DAG/chat_trace.json");

    return 0;
}
