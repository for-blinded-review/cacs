#include <assert.h>

#include "common/photonlib/callback.h"
#include "photonlib/co.h"
#include "photonlib/gen.h"
#include "stdgen.hpp"
#include <chrono>
#include <coroutine>
#include <cstdio>
#include <malloc.h>
#include <ranges>
#include <sys/mman.h>
#include <vector>

#include <boost/context/fiber.hpp>
#include <boost/coroutine2/all.hpp>

std::generator<int> towerOfHanoi_oldgen(int n, char from_rod, char to_rod,
                                        char aux_rod) {
  if (n == 0) {
    co_return;
  }
  for (auto x : towerOfHanoi_oldgen(n - 1, from_rod, aux_rod, to_rod))
    co_yield x;
  co_yield 1;
  for (auto x : towerOfHanoi_oldgen(n - 1, aux_rod, to_rod, from_rod))
    co_yield x;
}

std::generator<int> towerOfHanoi(int n, char from_rod, char to_rod,
                                 char aux_rod) {
  if (n == 0) {
    co_return;
  }
  co_yield std::ranges::elements_of(
      towerOfHanoi(n - 1, from_rod, aux_rod, to_rod));
  co_yield 1;
  co_yield std::ranges::elements_of(
      towerOfHanoi(n - 1, aux_rod, to_rod, from_rod));
}

struct arg {
  char n, f, t, a;
};

void recursive_towerOfHanoi(int n, char f, char t, char a,
                            Delegate<void, int> cb) {
  if (n == 0)
    return;
  recursive_towerOfHanoi(n - 1, f, a, t, cb);
  cb(1);
  recursive_towerOfHanoi(n - 1, a, t, f, cb);
}

void boost_towerOfHanoi(boost::coroutines2::coroutine<int>::push_type &yield,
                        int n, char f, char t, char a) {
  if (n == 0)
    return;
  boost_towerOfHanoi(yield, n - 1, f, a, t);
  yield(1);
  boost_towerOfHanoi(yield, n - 1, a, t, f);
}

void boost_context_towerOfHanoi(boost::context::fiber &yield, int n, char f,
                                char t, char a, int &x) {
  if (n == 0)
    return;
  boost_context_towerOfHanoi(yield, n - 1, f, a, t, x);
  x = 1;
  yield = std::move(yield).resume();
  boost_context_towerOfHanoi(yield, n - 1, a, t, f, x);
}

__attribute__((noinline)) __attribute__((preserve_none)) void
_Hanoi(GPromise &g, char n, char f, char t, char a) {
  if (n == 0)
    return;
  _Hanoi(g, n - 1, f, a, t);
  g.yield(1);
  _Hanoi(g, n - 1, a, t, f);
}

__attribute__((noinline)) __attribute__((preserve_none)) uint64_t
Hanoi(GCTX *fp, char n) {
  GPromise g(fp);
  _Hanoi(g, n, 'a', 'b', 'c');
  return 0;
}

uint64_t ss = 0;

void callback(void *, int x) { ss += x; }

static constexpr int ROUND = 10;
static_assert(ROUND >= 3, "Atleast needs 3 turns");

template <typename Func> void timeit(Func &&func, int round) {
  long long sp = 0;
  for (auto t = 0; t < round; t++) {
    auto spent = func();
    sp += spent;
  }
  printf("Average %lld us\n", sp / round);
}

int main() {
  photon::init();
  DEFER(photon::fini());

  auto __entry_time = std::chrono::high_resolution_clock::now();
  for (auto x : towerOfHanoi(1, 'a', 'c', 'b')) {
    ss++;
  }
  // for (auto level = 1; level <= 20; level++) {
  //   timeit(
  //       [&] {
  //         ss = 0;
  //         auto start = std::chrono::high_resolution_clock::now();
  //         for (auto x : towerOfHanoi_oldgen(level, 'a', 'c', 'b')) {
  //           ss++;
  //         }
  //         auto end = std::chrono::high_resolution_clock::now();
  //         printf(
  //             "l=%d\tcoro20gen\t%lld ns\t%lu\n", level,
  //             std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
  //                 .count(),
  //             ss);
  //         return std::chrono::duration_cast<std::chrono::nanoseconds>(end -
  //                                                                     start)
  //             .count();
  //       },
  //       ROUND);
  // }
  for (auto level = 1; level <= 20; level++) {
    timeit(
        [&] {
          ss = 0;
          Generator g(&Hanoi, (char)level);
          auto start = std::chrono::high_resolution_clock::now();
          for (; likely(g); g.resume()) {
            ss++;
          }
          auto end = std::chrono::high_resolution_clock::now();
          printf(
              "l=%d\tphotongen\t%lld ns\t%lu\n", level,
              std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                  .count(),
              ss);
          return std::chrono::duration_cast<std::chrono::nanoseconds>(end -
                                                                      start)
              .count();
        },
        ROUND);
  }
  for (auto level = 1; level <= 20; level++) {
    timeit(
        [&] {
          ss = 0;
          auto start = std::chrono::high_resolution_clock::now();
          for (auto x : towerOfHanoi(level, 'a', 'c', 'b')) {
            ss++;
          }
          auto end = std::chrono::high_resolution_clock::now();
          printf(
              "l=%d\tcoro23gen\t%lld ns\t%lu\n", level,
              std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                  .count(),
              ss);
          return std::chrono::duration_cast<std::chrono::nanoseconds>(end -
                                                                      start)
              .count();
        },
        ROUND);
  }
  for (auto level = 1; level <= 20; level++) {
    timeit(
        [&] {
          auto start2 = std::chrono::high_resolution_clock::now();
          uint64_t step = 0;
          recursive_towerOfHanoi(level, 'a', 'c', 'b',
                                 Delegate<void, int>{&callback, nullptr});
          auto end2 = std::chrono::high_resolution_clock::now();
          printf("l=%d\trecursion\t%lld ns\t%lu\n", level,
                 std::chrono::duration_cast<std::chrono::nanoseconds>(end2 -
                                                                      start2)
                     .count(),
                 step);
          return std::chrono::duration_cast<std::chrono::nanoseconds>(end2 -
                                                                      start2)
              .count();
        },
        ROUND);
  }
  for (auto level = 1; level <= 20; level++) {
    timeit(
        [&] {
          uint64_t step = 0;
          auto call = boost::coroutines2::coroutine<int>::pull_type(
              [&](boost::coroutines2::coroutine<int>::push_type &yield) {
                boost_towerOfHanoi(yield, level, 'a', 'c', 'b');
              });
          auto start2 = std::chrono::high_resolution_clock::now();
          for (auto x : call) {
            step += x;
          }
          auto end2 = std::chrono::high_resolution_clock::now();
          printf("l=%d\tpushpull\t%lld ns\t%lu\n", level,
                 std::chrono::duration_cast<std::chrono::nanoseconds>(end2 -
                                                                      start2)
                     .count(),
                 step);
          return std::chrono::duration_cast<std::chrono::nanoseconds>(end2 -
                                                                      start2)
              .count();
        },
        ROUND);
  }

  for (auto level = 1; level <= 20; level++) {
    timeit(
        [&] {
          uint64_t step = 0;
          int x;
          auto call = boost::context::fiber([&](boost::context::fiber &&yield) {
            boost_context_towerOfHanoi(yield, level, 'a', 'c', 'b', x);
            return std::move(yield);
          });
          auto start2 = std::chrono::high_resolution_clock::now();
          while (call) {
            call = std::move(call).resume();
            step += x;
          }
          auto end2 = std::chrono::high_resolution_clock::now();
          printf("l=%d\tb_context\t%lld us\t%lu\n", level,
                 std::chrono::duration_cast<std::chrono::nanoseconds>(end2 -
                                                                      start2)
                     .count(),
                 step);
          return std::chrono::duration_cast<std::chrono::nanoseconds>(end2 -
                                                                      start2)
              .count();
        },
        ROUND);
  }
  return 0;
}