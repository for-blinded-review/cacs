#pragma once

#include <coroutine>
#include <deque>
#include "common/photonlib/list.h"

template <typename T>
concept CoroTask = requires {
    T().handle.resume();
    T().handle.done();
    std::convertible_to<T, std::coroutine_handle<>>;
};

struct corofiber : public intrusive_list_node<corofiber>{
    std::coroutine_handle<> t;
};

struct Executor {
    intrusive_list<corofiber> q;
    uint64_t count;
    void schedule(std::coroutine_handle<> task);
    void exchange(std::coroutine_handle<> task);
    void yield() ;
    void execute() ;
    void done();
    void all();
    void clear() {
        count = 0;
    }
    uint64_t switch_count() {
        return count;
    }
    template <CoroTask T>
    void schedule(T&& t) {
        schedule(t.handle);
    }
};