#pragma once

#include "butil/status.h"
#include "bthread/bthread.h"
#include "bthread/butex.h"
#include "bthread/condition_variable.h"

#include <any>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>


namespace stream_dag {


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
        bthread_start_background(&tid_, nullptr, call_back, (void*)p_wrap_fn);
        joinable_ = true;
    }

    int join() {
        if (joinable_) {
            return bthread_join(tid_, NULL);
            joinable_ = false;
        }
        return -1;
    }

    bool joinable() const noexcept {
        return joinable_;
    }

    bthread_t get_tid() {
        return tid_;
    }

    void detach() {
        // if (joinable_) {
        //     bthread_detach(tid_);
        //     joinable_ = false;
        // }
    }

private:
    bthread_t tid_;
    bool joinable_ = false;
};


}