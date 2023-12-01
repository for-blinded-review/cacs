#include "common/photonlib/co.h"
#include "common/photonlib/utility.h"
#include <chrono>
#include <random>

static constexpr uint64_t THNUM = 10;
static constexpr uint64_t ROUND = 10'000'000;
static constexpr uint64_t TURN = 10;

void *yielder(void *arg) {
  for (int i = 0; i < INT_MAX; i++) {
    photon::thread_yield();
  }
  return 0;
}

int main() {
  photon::init();
  DEFER(photon::fini());
  for (int i = 0; i < THNUM; i++)
    photon::thread_create(yielder, nullptr);
  uint64_t yield = 0;
  uint64_t loop = 0;
  uint64_t maxs = 0;
  uint64_t mins = -1;
  uint64_t maxi = 0;
  uint64_t mini = -1;
  for (int i = 0; i < TURN; i++) {
    auto start_yield = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ROUND; i++) {
      photon::thread_yield();
    }
    auto start_loop = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ROUND * THNUM; i++) {
      asm volatile(""
                   :
                   :
                   : "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "r8",
                     "r9", "r10", "r11", "r12", "r13", "r14", "r15");
    }
    auto emptyloop = std::chrono::high_resolution_clock::now();
    auto total_test = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      (start_loop - start_yield))
                      .count();

    auto total_interrupt = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           (emptyloop - start_loop))
                           .count();

    yield += (total_test - total_interrupt) / ROUND;
    loop += total_interrupt / ROUND;
  }
  printf("Average yield %lf loop %lf\n",
         (yield - maxs - mins) * 1.0 / (TURN - 2) / THNUM,
         (loop - maxi - mini) * 1.0 / (TURN - 2) / THNUM);
  return 0;
}