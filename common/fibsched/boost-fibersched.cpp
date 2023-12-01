#include "boost-fibersched.h"

void FiberSched::yield() { set_back(get_next()->resume()); }
void FiberSched::all() {
  while (__builtin_expect(!q.node->single(), 1)) {
    yield();
  }
}