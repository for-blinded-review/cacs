#pragma once

#include "common/photonlib/list.h"
#include <boost/context/fiber.hpp>
#include <deque>
#include <iostream>

struct fibnode : public intrusive_list_node<fibnode> {
  boost::context::fiber fiber;

  fibnode(boost::context::fiber &&f) : fiber(std::move(f)) {}
  fibnode() = default;

  boost::context::fiber resume() { return std::move(fiber).resume(); }
};

class FiberSched {
protected:
  using fiber = boost::context::fiber;
  intrusive_list<fibnode> q;
  uint64_t count;

  void set_back(fiber &&t) {
    assert(!q.back()->fiber);
    if (t) {
      q.back()->fiber = std::move(t);
    }
  }
  auto get_next() {
    q.node = q.node->next();
    return q.node;
  }
  fiber && final() {
    // current node is now finished
    auto th = q.pop_front();
    delete th;
    count++;
    return std::move(q.node->fiber);
  }

public:
  FiberSched() { q.push_front(new fibnode()); }

  ~FiberSched() { q.delete_all(); }

  template <typename Func>
  void schedule(Func &&task) {
    q.push_back(new fibnode(fiber([this, task](fiber &&f) {
      set_back(std::move(f));
      task();
      return final();
    })));
  }
  void yield();
  void all();
  uint64_t switch_count() { return count; }
  void clear() { count = 0; }
};