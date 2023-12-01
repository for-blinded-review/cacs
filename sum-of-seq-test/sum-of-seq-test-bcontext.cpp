#include <boost/coroutine2/all.hpp>
#include <stdio.h>
#include <chrono>
#include <ratio>

/// Search for --- to skip over generator code

#include <coroutine>

template <typename T> struct generator {
  struct promise_type {
    T current_value;
    std::suspend_always yield_value(T value) {
      this->current_value = value;
      return {};
    }
    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    generator get_return_object() { return generator{this}; };
    void unhandled_exception() { std::terminate(); }
    void return_void() {}
  };

  struct iterator {
    std::coroutine_handle<promise_type> _Coro;
    bool _Done;

    iterator(std::coroutine_handle<promise_type> Coro, bool Done)
        : _Coro(Coro), _Done(Done) {}

    iterator &operator++() {
      _Coro.resume();
      _Done = _Coro.done();
      return *this;
    }

    bool operator==(iterator const &_Right) const {
      return _Done == _Right._Done;
    }

    bool operator!=(iterator const &_Right) const { return !(*this == _Right); }
    T const &operator*() const { return _Coro.promise().current_value; }
    T const *operator->() const { return &(operator*()); }
  };

  iterator begin() {
    p.resume();
    return {p, p.done()};
  }

  iterator end() { return {p, true}; }

  generator(generator const&) = delete;
  generator(generator &&rhs) : p(rhs.p) { rhs.p = nullptr; }

  ~generator() {
    if (p)
      p.destroy();
  }

private:
  explicit generator(promise_type *p)
      : p(std::coroutine_handle<promise_type>::from_promise(*p)) {}

  std::coroutine_handle<promise_type> p;
};

//// --- stackful coroutines

typedef boost::coroutines2::coroutine<uint64_t> coro_t;

void producer(coro_t::push_type &sink, uint64_t count) {
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

void test_boost_coro(uint64_t iters) {
  puts("testing stackful... ");
  coro_t::pull_type p([iters](auto& out) { producer(out, iters); });
  auto result = reduce(p);
  printf("=> %lu\n", result);
}

typedef boost::context::fiber fiber;

struct bcproducer {
  uint64_t x;
  fiber fib;

  bcproducer(uint64_t count)
      : fib([this, count](fiber &&sink) mutable {
          for (; count != 0; --count) {
            x = count;
            sink = std::move(sink).resume();
          }
          x = 0;
          return std::move(sink);
        }) {}
};

uint64_t reduce(bcproducer& source) {
  uint64_t sum = 0;
  while (source.fib) {
    source.fib = std::move(source.fib).resume();
    sum += source.x;
  }
  return sum;
}


__attribute__((noinline)) 
void boost_fiber(uint64_t iters) {
  puts("testing stackful boost context... ");
  bcproducer p(iters);
  auto result = reduce(p);
  printf("=> %lu\n", result);
}
//// --- stackless coroutines

// // comment out noinline and see what happens
__attribute__((noinline)) 
generator<uint64_t> producer(uint64_t count) {
  for (; count != 0; --count)
    co_yield count;

}
uint64_t reduce(generator<uint64_t> &source)
{
  uint64_t sum = 0;
  for (auto v: source)
    sum += v;
  return sum;
}

__attribute__((noinline)) 
void boost_coro20(uint64_t iters) {
  puts("testing stackless... ");
  auto p = producer(iters);
  auto result = reduce(p);
  printf("=> %lu\n", result);
}

using namespace std::chrono;

using dur = duration<double>;
using hpc = high_resolution_clock;

int main() {
  int count = 100'000;
  int nano = 1'000'000'000;
  try {
    dur elapsed1, elapsed2, elapsed3;
    {
      auto start = hpc::now();
      test_boost_coro(count);
      auto stop = hpc::now();
      elapsed1 = duration_cast<dur>(stop - start);
      printf("elapsed %g ms\n", elapsed1.count()*1000);
    }
    {
      auto start = hpc::now();
      boost_fiber(count);
      auto stop = hpc::now();
      elapsed3 = duration_cast<dur>(stop - start);
      printf("elapsed %g ms\n", elapsed3.count()*1000);
    }
    {
      auto start = hpc::now();
      boost_coro20(count);
      auto stop = hpc::now();
      elapsed2 = duration_cast<dur>(stop - start);
      printf("elapsed %g\n", elapsed2.count()*1000);
    }
    printf("stackless are %g times faster\n", elapsed3.count() / elapsed2.count());
    printf("stackful context consume %g ns per iteration\n", elapsed3.count() / count * nano);
    printf("stackless consume %g ns per iteration\n", elapsed2.count() / count * nano);
  } catch (...) {
    puts("caught something");
  }
}
