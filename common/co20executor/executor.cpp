#include <coroutine>
#include "executor.h"

void Executor::schedule(std::coroutine_handle<> task) {
    auto f = new corofiber;
    f->t = task;
    q.push_back(f);
}
void Executor::exchange(std::coroutine_handle<> task) {
    q.node->t = task;
}
void Executor::done() {
    auto t = q.pop_front();
    delete t;
}
void Executor::execute() {
    count ++;
    q.node->t.resume();
}
void Executor::yield() {
    q.node = q.node->next();
}
void Executor::all() {
    while (!q.empty()) {
        execute();
    }
}
