#pragma once
#include <any>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "common.h"
#include "stream.h"
#include <nlohmann/json.hpp>

namespace stream_dag {


template <class T>
struct when_any_args {
    Stream<T>* t;
    int* stop_flag;
    bthread::ConditionVariable* cond;
    Status* status;
    T* result;
};

template <class T1, class T2>
std::tuple<std::optional<T1>, std::optional<T2>> when_any(Stream<T1>& t1, Stream<T2>& t2) {
    bthread::ConditionVariable cond;
    bthread::Mutex mutex;

    int stop_flag = 0;
    Status status1, status2;

    bthread_t bid1, bid2;
    T1 result1;
    T2 result2;

    when_any_args<T1> args1 {
        .t = &t1,
        .stop_flag = &stop_flag,
        .cond = &cond,
        .status = &status1,
    };

    when_any_args<T2> args2 {
        .t = &t2,
        .stop_flag = &stop_flag,
        .cond = &cond,
        .status = &status2,
    };

    bthread_start_background(&bid1, nullptr, [](void* args) ->void* {
        when_any_args<T1>* pargs = (when_any_args<T1>*) args;
        *pargs->status = pargs->t->wait();
        *pargs->stop_flag = 1;
        pargs->cond->notify_one();
        return nullptr;
    }, &args1);


    bthread_start_background(&bid2, nullptr, [](void* args) ->void* {
        when_any_args<T2>* pargs = (when_any_args<T2>*) args;
        *pargs->status = pargs->t->wait();
        *pargs->stop_flag = 2;
        pargs->cond->notify_one();
        return nullptr;
    }, &args2);

    std::unique_lock<bthread::Mutex> lock(mutex);
    while (stop_flag == 0 && !t1.readable() && !t2.readable()) {
        cond.wait(lock);
    }
    
    bthread_interrupt(bid1);
    bthread_interrupt(bid2);
    bthread_join(bid1, nullptr);
    bthread_join(bid2, nullptr);

    
    if (stop_flag == 1) {
        if (t1.readable()) {
            T1 result;
            t1.read(result);

            return std::tuple<std::optional<T1>, std::optional<T2>>{{result}, {}};
        }
        return std::tuple<std::optional<T1>, std::optional<T2>>{{}, {}};
    }

    if (stop_flag == 2) {
        if (t2.readable()) {
            T2 result;
            t2.read(result);

            return std::tuple<std::optional<T1>, std::optional<T2>>{{}, {result}};
        }
        return std::tuple<std::optional<T1>, std::optional<T2>>{{}, {}};
    }

    return std::tuple<std::optional<T1>, std::optional<T2>>{{}, {}};
}


}