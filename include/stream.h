#pragma once
#include "common.h"
#include "to_json.h"
#include "bthread/butex.h"
#include "bthread/condition_variable.h"
#include <memory>
#include <tuple>
#include <nlohmann/json.hpp>

namespace stream_dag {
using json = nlohmann::json;
using Status = butil::Status;


class BaseData {
public:
    BaseData(BaseContext& ctx, const std::string& name, const std::string& type) : ctx_(ctx), name_(name), type_(type) {}
    virtual ~BaseData() = default;

    void close() {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        trace("BaseData::close", json());
        closed_ = true;
        lock_.unlock();
        cond_.notify_one();
    }

    bool is_close() {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        return closed_;
    }
    
    // 写结束，但是可读
    void half_close(Status status=Status::OK()) {
        std::unique_lock<bthread::Mutex> lock_(mutex_);

        json trace_info;
        trace_info["code"] = status.error_code();
        trace_info["msg"] = status.error_str();
        trace("BaseData::half_close", trace_info);

        half_closed_ = true;
        lock_.unlock();
        cond_.notify_one();
    }

    bool is_half_close() {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        return half_closed_;
    }

    void trace(const std::string& event, json value) {
        ctx_.trace_stream(name_, type_, event, value);
    }

public:
    BaseContext& ctx_;
    std::string name_, type_;

    bool half_closed_ = false;
    bool closed_ = false;

    bthread::ConditionVariable cond_;
    bthread::Mutex mutex_;
};

template<class T>
class Data : public BaseData {
public:
    Data(BaseContext& ctx, const std::string& name, const std::string& type) : BaseData(ctx, name, type) {}
    virtual ~Data() = default;

    void set(T&& data) {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        trace("Data::set", json());
        data_ = std::move(data);
        lock_.unlock();
        cond_.notify_one();
    }

    Status get(T& data) {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        while (/*set_ != true && */ !closed_ && !half_closed_) {
            trace("InputData::read wait", json());
            int rc = cond_.wait_for(lock_, 1000000);
            if (rc != 0) {
                return Status(-1, "InputData::read wait");
            }
            trace("InputData::read wake", json({{"rc", rc}}));
        }
        data = data_;
        return Status::OK();
    }

    T& get() {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        while (/*set_ != true && */ !closed_ && !half_closed_) {
            trace("InputData::read wait", json());
            int rc = cond_.wait_for(lock_, 1000000);
            if (rc != 0) {
                throw std::runtime_error("InputData::read wait failed");
            }
            trace("InputData::read wake", json({{"rc", rc}}));
        }
        return data_;
    }

public:
    T data_;
    // bool set_ = false;
};

template <class T>
class OutputData : public Data<T> {
public:
    using Data<T>::Data;
    T* operator->() {
        std::unique_lock<bthread::Mutex> lock_(this->mutex_);
        return &this->data_;
    }

    T& operator*() {
        std::unique_lock<bthread::Mutex> lock_(this->mutex_);
        return this->data_;
    }
};

// template<class T>
// class InputData : public Data<T> {
// public:
//     using Data<T>::Data;
//     T* operator->() {
//         std::unique_lock<bthread::Mutex> lock_(this->BaseData::mutex_);
//         // 如果判断 set 状态，可能 set 不完整就被读取了，会和直觉不符
//         while ( /*set_ != true && */ !this->BaseData::closed_ && !this->BaseData::half_closed_) {
//             this->trace("InputData::read wait", json());
//             int rc = this->cond_.wait_for(lock_, 1000000);
//             if (rc != 0) {
//                 this->trace("InputData::read wait fail", json({{"rc", rc}}));
//                 return nullptr;
//             }
//             this->trace("InputData::read wake", json({{"rc", rc}}));
//         }
//         return &this->data_;
//     }

//     T& operator*() {
//         std::unique_lock<bthread::Mutex> lock_(this->BaseData::mutex_);
//         // 如果判断 set 状态，可能 set 不完整就被读取了，会和直觉不符
//         while ( /*set_ != true && */ !this->BaseData::closed_ && !this->BaseData::half_closed_) {
//             this->trace("InputData::read wait", json());
//             int rc = this->cond_.wait_for(lock_, 1000000);
//             if (rc != 0) {
//                 this->trace("InputData::read wait fail", json({{"rc", rc}}));
//                 return this->data_;
//             }
//             this->trace("InputData::read wake", json({{"rc", rc}}));
//         }
//         return this->data_;
//     }
// };


template<class T>
class InputData {
public:
    InputData(BaseContext& ctx, const std::string& name, const std::string& type) : ctx_(ctx), name_(name), type_(type) {}
    InputData(Data<T>& output) : output_(output) {}
    
    T* operator->() {
        return &output_.get();
    }

    T& operator*() {
        return output_.get();
    }

    void half_close() {
        output_.half_close();
    }

private:
    BaseContext& ctx_;
    std::string name_, type_;

    Data<T>& output_;
};


enum class StreamStatus {
    // 未初始化
    UNINIT = 0,
    // 读结束
    READ_END = 1,
    // 写结束
    WRITE_END = 2,
    // 读写结束
    READ_WRITE_END = 3,
    // 读写未结束
    READ_WRITE = 4,
    // 读写未结束，但是写结束
};

class PipeStreamBase {
public:
    PipeStreamBase(BaseContext& ctx, const std::string& name, const std::string& type) : ctx_(ctx), name_(name), type_(type) {}
    virtual ~PipeStreamBase() = default;

    void close() {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        trace("PipeStreamBase::close", json());
        closed_ = true;
        // for (auto& it : callback_) {
        //     it.second(Status(2, "close"));
        // }
        lock_.unlock();
        cond_.notify_one();
    }

    bool is_close() {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        return closed_;
    }
    
    // 写结束，但是可读
    void half_close(Status status=Status::OK()) {
        std::unique_lock<bthread::Mutex> lock_(mutex_);

        json trace_info;
        trace_info["code"] = status.error_code();
        trace_info["msg"] = status.error_str();
        trace("PipeStreamBase::half_close", trace_info);
        half_closed_ = true;
        // for (auto& it : callback_) {
        //     it.second(Status(1, "half_close"));
        // }
        lock_.unlock();
        cond_.notify_one();
    }

    bool is_half_close() {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        return half_closed_;
    }

    void trace(const std::string& event, json value) {
        ctx_.trace_stream(name_, type_, event, value);
    }

protected:
    BaseContext& ctx_;
    std::string name_, type_;

    bool half_closed_ = false;
    bool closed_ = false;

    bthread::ConditionVariable cond_;
    bthread::Mutex mutex_;

    // callback 很难做到协程安全。废弃 callback 实现.
    // int callback_id_ = 1000;
    // std::unordered_map<int, std::function<void(Status)>> callback_;


};

template<class T>
class PipeStream : public PipeStreamBase {
public:
    using PipeStreamBase::PipeStreamBase;

    Status append(T&& data) {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        trace("PipeStreamBase::append", to_json(data));
        buf_.push_back(data);
        // for (auto& it : callback_) {
        //     it.second(Status::OK());
        // }
        cond_.notify_one();
        lock_.unlock();
        return Status::OK();
    }
    Status append(T& data) {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        trace("PipeStreamBase::append", data.to_json());
        buf_.push_back(data);
        // for (auto& it : callback_) {
        //     it.second(Status::OK());
        // }
        cond_.notify_one();
        lock_.unlock();
        return Status::OK();
    }

    Status read(T& result) {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        while (buf_.size() <= top_ && !closed_ && !half_closed_) {
            trace("PipeStreamBase::read wait", json());
            int rc = cond_.wait_for(lock_, 1000000);
            if (rc != 0) {
                return Status(-1, "PipeStreamBase::read wait");
            }
            trace("PipeStreamBase::read wake", json({{"rc", rc}}));
        }
        if (top_ < buf_.size()) {
            trace("PipeStreamBase::read buf", json());
            result = buf_[top_];
            top_ += 1;
            return Status::OK();
        }
        if (closed_) {
            return Status(2, "PipeStreamBase::read closed");
        }
        if (half_closed_) {
            return Status(1, "PipeStreamBase::read half_closed");
        }

        return Status::OK();
    }

    std::tuple<Status, T> read() {
        T result;
        auto status = read(result);
        return {status, result};
    }

    Status wait() {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        while (buf_.size() <= top_ && !closed_ && !half_closed_) {
            trace("PipeStreamBase::wait wait", json());
            int rc = cond_.wait_for(lock_, 1000000);
            if (rc != 0) {
                return Status(-1, "PipeStreamBase::wait wait");
            }
            trace("PipeStreamBase::wait wake", json({{"rc", rc}}));
        }
        if (top_ < buf_.size()) {
            trace("PipeStreamBase::wait ok", json());
            return Status::OK();
        }
        if (closed_) {
            return Status(2, "PipeStreamBase::wait closed");
        }
        if (half_closed_) {
            return Status(1, "PipeStreamBase::wait half_closed");
        }

        return Status::OK();
    }

    bool has_data() {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        return top_ < buf_.size();
    }

    bool readable() {
        std::unique_lock<bthread::Mutex> lock_(mutex_);
        return top_ < buf_.size() || (!closed_ && !half_closed_);
    }

private:


    std::vector<T> buf_;
    int top_ = 0;

};

template<class T>
using Stream = PipeStream<T>;

}