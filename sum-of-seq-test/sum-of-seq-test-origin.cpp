#include <boost/coroutine2/all.hpp>
#include <stdio.h>
#include <chrono>
#include <ratio>

/// Search for --- to skip over generator code

#include <coroutine>
#include "g.h"

typedef boost::coroutines2::coroutine<int> coro_t;

void producer(coro_t::push_type &sink, int count) {
  for (; count != 0; --count)
    sink(count);
}

int reduce(coro_t::pull_type &source)
{
  int sum = 0;
  for (auto v: source)
    sum += v;
  return sum;
}

void test_boost_coro(int iters) {
  puts("testing stackful... ");
  coro_t::pull_type p([iters](auto& out) { producer(out, iters); });
  auto result = reduce(p);
  printf("=> %d\n", result);
}

int reduce(generator<int> &source)
{
  int sum = 0;
  for (auto v: source)
    sum += v;
  return sum;
}

// comment out noinline and see what happens
__attribute__((noinline)) 
void test_coro20(int iters) {
  puts("testing stackless... ");
  auto p = producer(iters);
  auto result = reduce(p);
  printf("=> %d\n", result);
}

using namespace std::chrono;

using dur = duration<double>;
using hpc = high_resolution_clock;

int main() {
  int count = 100'000;
  int nano = 1'000'000'000;
  try {
    dur elapsed1, elapsed2;
    {
      auto start = hpc::now();
      test_boost_coro(count);
      auto stop = hpc::now();
      elapsed1 = duration_cast<dur>(stop - start);
      printf("elapsed %g ms\n", elapsed1.count()*1000);
    }
    {
      auto start = hpc::now();
      test_coro20(count);
      auto stop = hpc::now();
      elapsed2 = duration_cast<dur>(stop - start);
      printf("elapsed %g\n", elapsed2.count()*1000);
    }
    printf("stackless are %g times faster\n", elapsed1.count() / elapsed2.count());
    printf("stackful consume %g ns per iteration\n", elapsed1.count() / count * nano);
    printf("stackless consume %g ns per iteration\n", elapsed2.count() / count * nano);
  } catch (...) {
    puts("caught something");
  }
}
