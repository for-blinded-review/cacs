#include <stdio.h>
#include <inttypes.h>
#include <utility>
#include <new>
#include "common/photonlib/gen.h"

__attribute__((noinline))
__attribute__((preserve_none))
uint64_t seq_gen(GCTX* fp, uint64_t c) {
    GPromise g(fp);
    while(c) {
        g.yield(c--);
    }
    return 0;
}

uint64_t make_move(uint64_t n, uint64_t f, uint64_t t) {
    return n + (f << 8) + (t << 16);
}

__attribute__((noinline))
__attribute__((preserve_none))
void _Hanoi(GPromise& g, char n, char f, char t, char a) {
    if (n == 0) return;
    _Hanoi(g, n-1, f, a, t);
    g.yield(make_move(n, f, t));
    _Hanoi(g, n-1, a, t, f);
}

__attribute__((noinline))
__attribute__((preserve_none))
uint64_t Hanoi(GCTX* fp, char n) {
    GPromise g(fp);
    _Hanoi(g, n, 'a', 'b', 'c');
    return 0;
}

__attribute__((noinline))
__attribute__((preserve_none))
uint64_t test_hanoi(uint64_t c) {
    uint64_t sum = 0;
    Generator g(&Hanoi, (char)c);
    for (;likely(g); g.resume()) {
        sum++;
    }
    return sum;
}

__attribute__((noinline))
__attribute__((preserve_none))
uint64_t test_sequence_gen(uint64_t c) {
    uint64_t sum = 0;
    Generator g(&seq_gen, c);
    for (;likely(g); g.resume())
        sum += g.value();
    return sum;
}

