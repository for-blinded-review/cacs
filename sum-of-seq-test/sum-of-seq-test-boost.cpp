#include <boost/coroutine2/all.hpp>
#include <stdio.h>
#include <chrono>
#include <ratio>
#include <utility>

typedef boost::coroutines2::coroutine<int> coro_t;

void producer(coro_t::push_type &sink, int count) {
  for (; count != 0; --count)
    sink(count);
}

uint64_t reduce(coro_t::pull_type &source)
{
  uint64_t sum = 0;
  for (auto v: source)
    sum += v;
  return sum;
}

void test_boost_coro(int iters) {
  puts("testing stackful... ");
  coro_t::pull_type p([iters](auto& out) { producer(out, iters); });
  auto result = reduce(p);
  printf("=> %llu\n", result);
}

void test_boost_context_fiber(int iters) {
  namespace ctx=boost::context;
  int a = iters;
  ctx::fiber source{[&a](ctx::fiber&& sink){
      for(; a; --a){
          sink=std::move(sink).resume();
      }
      return std::move(sink);
  }};
  uint64_t sum = 0;
  while(source) {
    source=std::move(source).resume();
    sum += a;
  }
  printf("=> %llu\n", sum);
}

using namespace std::chrono;

using dur = duration<double>;
using hpc = high_resolution_clock;

int main() {
  int count = 10'000'000;
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
      test_boost_context_fiber(count);
      auto stop = hpc::now();
      elapsed2 = duration_cast<dur>(stop - start);
      printf("elapsed %g\n", elapsed2.count()*1000);
    }
    printf("boost.coroutine2 consume %g ns per iteration\n", elapsed1.count() / count * nano);
    printf("boost.context consume %g ns per iteration\n", elapsed2.count() / count * nano);
    printf("boost.context are %g times faster\n", elapsed1.count() / elapsed2.count());
  } catch (...) {
    puts("caught something");
  }
}
