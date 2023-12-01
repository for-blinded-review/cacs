#pragma once

#include <coroutine>

#include "executor.h"

struct Yield {};

static Executor executor;

struct YieldAwaiter {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) noexcept  {
        executor.yield();
    }
    void await_resume() noexcept {}
};

template <typename ValType>
struct Task {
    struct promise_type {
        auto get_return_object() noexcept  {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::coroutine_handle<> caller = {};
        ValType store;

        auto initial_suspend() noexcept { return std::suspend_always{}; }

        template <std::convertible_to<ValType> T>
        std::suspend_never return_value(T &&x) noexcept  {
            store = std::move(x);
            if (caller) {
                executor.exchange(caller);
            } else {
                executor.done();
            }
            std::atomic_signal_fence(std::memory_order_release);
            return {};
        }
        auto final_suspend() noexcept { return std::suspend_never{}; }
        void unhandled_exception() noexcept  { exit(1); }

        YieldAwaiter await_transform(Yield) noexcept  { return {}; }

        template <CoroTask T>
        auto await_transform(T &&expr)  noexcept {
            auto hd = expr.handle;
            expr.handle.promise().caller =
                std::coroutine_handle<promise_type>::from_promise(*this);
            executor.exchange(hd);
            return std::move(expr);
        }
    };

    std::coroutine_handle<promise_type> handle;

    bool await_ready()  noexcept { return false; }

    void await_suspend(std::coroutine_handle<>)  noexcept {}

    ValType await_resume()  noexcept { return std::move(handle.promise().store); }
};