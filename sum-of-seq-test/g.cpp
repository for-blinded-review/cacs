#include "g.h"

__attribute__((noinline))
generator<int> producer(int count) {
  while((count)) {
    co_yield (count--);
  }
}

__attribute__((noinline))
generator<int> producer2(int count) {
  for (auto x: producer(count)) {
    co_yield x;
  }
}

__attribute__((noinline))
generator<int> producer3(int count) {
  for (auto x: producer2(count)) {
    co_yield x;
  }
}

