#pragma once
#include <utility>
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

class NoCopyNoMove {
protected:
    NoCopyNoMove() = default;
    NoCopyNoMove(const NoCopyNoMove&) = delete;
    NoCopyNoMove(NoCopyNoMove&&) = delete;
    void operator = (const NoCopyNoMove&) = delete;
    void operator = (NoCopyNoMove&&) = delete;
};

struct GCTX {
    uint64_t _ip, _fp;
};

class Generator : public NoCopyNoMove {
    GCTX _ctx;
    uint64_t _val;
public:
    template<typename F, typename...Ts>
    __attribute__((always_inline))
    Generator(F&& f, Ts&&...xs) {
        auto fp = __builtin_frame_address(0);
        _val = f((GCTX*)fp, std::forward<Ts>(xs)...);
        register uint64_t callee_ip asm("rdx");
        register uint64_t callee_fp asm("rsi");
        asm volatile ("" : "=r"(callee_ip), "=r"(callee_fp) : :
            "rbx", "rcx", "rsp", "r8", "r9", "r10", "r11",
            "r12", "r13", "r14", "r15" );
        _ctx = {callee_ip, callee_fp};
    }

    __attribute__((always_inline))
    void resume() {
        register uint64_t& _ip asm("rdx") = _ctx._ip;
        register uint64_t& _fp asm("rsi") = _ctx._fp;
        register uint64_t retval asm("rax");
        __asm__ volatile (R"(
            lea 1f(%%rip), %%rdi
            xchg %%rbp, %0
            jmp *%1
        1:
        )" : "+r"(_fp), "+r"(_ip), "=r"(retval) : :
             "rbx", "rcx", "rsp", "rdi", "r8", "r9",
             "r10", "r11", "r12", "r13", "r14", "r15");
        _val = retval;
    }
    uint64_t value() {
        return _val;
    }
    operator bool() {
        return _ctx._ip;
    }
    ~Generator() {
        while(unlikely(*this))
            resume();
    }
};

class GPromise : public NoCopyNoMove {
    GCTX _ctx;
public:
    GPromise(GCTX* fp) {
        _ctx._ip = (uint64_t)__builtin_return_address(0);
        _ctx._fp = (uint64_t)fp;
    }
    __attribute__((always_inline))
    void yield(uint64_t x) {
        register uint64_t& retval asm("rax") = x;
        register uint64_t& _caller_ip asm("rdi") = _ctx._ip;
        register uint64_t& _caller_fp asm("rsi") = _ctx._fp;
        __asm__ volatile (R"(
            lea 1f(%%rip), %%rdx
            xchg %%rbp, %1
            jmp *%0     // when switch out: rax: retval, rdx: callee ip, rsi: callee bp
            1:          // when switch  in: rdi (same reg): caller ip, rsi (same reg): caller bp
        )" : "+r"(_caller_ip), "+r"(_caller_fp), "+r"(retval) :
           : "rbx", "rcx", "rdx", "rsp", "r8", "r9",
             "r10", "r11", "r12", "r13", "r14", "r15");
    }
    ~GPromise() {
        auto frame = (uint64_t*)__builtin_frame_address(0);
        frame[1] = _ctx._ip;
        register uint64_t _callee_ip asm("rdx");
        __asm__ volatile ("xor %0, %0" : "=r"(_callee_ip));
    }
};
