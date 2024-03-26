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


}