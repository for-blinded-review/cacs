#pragma once

inline __attribute__((always_inline))
void CACS(void* from, void* to) {
    asm volatile (R"(
        lea 1f(%%rip), %%rax
        push %%rax
        mov %%rsp, 0x10(%0)     # save rsp
        mov 0x10(%1), %%rsp     # load rsp
        pop %%rax
        jmp *%%rax
        1:
    )" : : "r"(from), "r"(to) :
        "rax", "rbx", "rcx", "rdx", //"rsi", "rdi",
        "rbp", "r8", "r9", "r10", "r11", "r12",
        "r13", "r14", "r15");
}

inline __attribute__((always_inline))
void CACS_die_switch_defer(void* from, void* to, void(*defer)(void*)) {
    register auto f asm ("rdi") = from;
    asm volatile (R"(
        mov  0x10(%1), %%rsp     # load rsp
        jmpq  *%2
    )" : : "r"(f), "r"(to), "r"(defer) : "rax");
}

inline __attribute__((always_inline))
void CACS_tail(void* from, void* to) {
    asm volatile (R"(
        mov %%rsp, 0x10(%0)     # save rsp
        mov 0x10(%1), %%rsp     # load rsp
        pop %%rax
        jmp *%%rax
    )" : : "r"(from), "r"(to) :
        "rax", "rbx", "rcx", "rdx", //"rsi", "rdi",
        "rbp", "r8", "r9", "r10", "r11", "r12",
        "r13", "r14", "r15");
}

