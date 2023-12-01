#include <stdio.h>
#include <chrono>
#include <ratio>
#include <sys/time.h>
#include "g.h"


generator<uint64_t> producer(uint64_t count);


__attribute__((noinline))
generator<uint64_t> producer0(uint64_t count) {
  while((count)) {
    co_yield count--;
  }
}

uint64_t reduce(generator<uint64_t> &source)
{
  uint64_t sum = 0;
  for (auto v: source)
    sum += v;
  return sum;
}

// comment out noinline and see what happens
__attribute__((noinline))
uint64_t test2level(uint64_t iters) {
  // puts("testing stackless... ");
  auto p = producer0(iters);
  auto result = reduce(p);
  // printf("sum=%llu\n", result);
  return result;
}

using namespace std::chrono;

using dur = duration<double>;
using hpc = high_resolution_clock;

__attribute__((preserve_none))
uint64_t test_sequence_gen(uint64_t c);

__attribute__((preserve_none))
uint64_t test_hanoi(uint64_t c);

inline struct timeval now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv;
}

inline double sub(struct timeval a, struct timeval b) {
  auto sec = a.tv_sec - b.tv_sec;
  auto usec = a.tv_usec - b.tv_usec;
  return sec + usec / 1e6;
}

int main(int argc, char** argv) {
  const int count = 1'000'000'000;
  const int nano  = 1'000'000'000;

  uint64_t padding[1] = {0};

  try {
    uint64_t R;
    const char* name;
    auto t0 = now();
    if (argc == 2) {
      if (argv[1][0] == 'x') {
        R = test_sequence_gen(count);
        name = "ful";
      } else if (argv[1][0] == 'y') {
        R = test_hanoi(20);
        name = "ful nested generator (Hanoi)";
      } else goto t2;
    } else {
    t2:
      R = test2level(count);
      name = "less";
    }
    auto t1 = now();
    printf("stack%s consume %g ns per iteration\n"
           "Result=%lu\n", name, sub(t1, t0), R);
  } catch (...) {
    puts("caught something");
  }
  puts((char*)padding);
  return (int)padding[0];
}
