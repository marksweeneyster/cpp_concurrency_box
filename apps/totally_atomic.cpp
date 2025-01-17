#include "atomics/lock_free_stack.h"

#include <random>
#include <thread>

int main() {
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(1, 1000);

  totally_atomic::lock_free_stack<int> lock_free_stack;
  //totally_atomic::LockFreeStack<int> lock_free_stack;

  for (int ii = 0; ii < 100; ++ii) {
    std::thread t([&]() {
      for (int jj = 0; jj < 10'000; ++jj) {
        lock_free_stack.push(dist(rng));
      }
    });
    t.detach();
  }

  //int val = -1;
  //lock_free_stack.pop(val);
  auto val = lock_free_stack.pop();

  unsigned int sz = 100;
  std::vector<std::thread> tvec;
  tvec.reserve(sz);
  for (auto ii = 0U; ii < sz; ++ii) {
    tvec.emplace_back([&lock_free_stack]() {
      while (lock_free_stack.pop()) {}
      //int valz = -1;
      //while (lock_free_stack.pop(valz)) {}
    });
  }

  for (auto& popper: tvec) {
    popper.join();
  }

  return val ? *val : -1;
  //return val;
}
