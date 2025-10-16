#include "atomics/lock_free_stack.h"

#include <iostream>
#include <random>
#include <thread>

void worker_fn(totally_atomic::lock_free_stack<int>& lock_free_stack) {
  thread_local std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(500, 10000);

  for (int jj = 0; jj < 10; ++jj) {
    lock_free_stack.push(dist(rng));
  }
}

int main() {
  {
    std::cout << "Node size: " << totally_atomic::lock_free_stack<int>::node_size() << "\n";
    //thread_local std::mt19937 rng(std::random_device{}());
    //std::uniform_int_distribution<int> dist(500, 10000);

    totally_atomic::lock_free_stack<int> lock_free_stack;

    constexpr auto sz = 10U;
    std::vector<std::thread> pushers;
    pushers.reserve(sz);

    for (auto ii = 0U; ii < sz; ++ii) {
      pushers.emplace_back([&]() { worker_fn(lock_free_stack); });
    }

    for (auto& pusher: pushers) {
      pusher.join();
    }

    auto val = lock_free_stack.pop();

    std::vector<std::thread> poppers;
    poppers.reserve(sz);
    for (auto ii = 0U; ii < sz; ++ii) {
      poppers.emplace_back([&lock_free_stack]() {
        while (lock_free_stack.pop()) {}
      });
    }

    for (auto& popper: poppers) {
      popper.join();
    }

    std::cout << "sptr value: " << (val ? *val : -1) << "\n";
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
