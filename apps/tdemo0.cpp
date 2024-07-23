#include "apricot/int_thread.hpp"
#include "apricot/queue.hpp"
#include <future>
#include <iostream>

int main(int argc, char* argv[]) {
  uint16_t num_tasks = 11;

  using task_t   = std::packaged_task<int()>;
  using future_t = std::future<int>;

  apricot::Queue1<task_t> task_queue;

  std::vector<task_t> tasks(num_tasks);
  std::vector<future_t> results(num_tasks);

  std::vector<apricot::SmartThread> consumers;

  int not_random_thread_index = num_tasks / 2;

  for (int i = 0; i < num_tasks; ++i) {
    consumers.emplace_back([&task_queue] {

      auto task_opt = task_queue.dequeue();
      if (task_opt) {
        auto& task = task_opt.value();
        // do the work
        task();
      }
    });
    if (i == not_random_thread_index) {
      consumers[not_random_thread_index].interrupt();
    }
  }

  for (int i = 0; i < num_tasks; ++i) {
    if (i == not_random_thread_index) {
      tasks[i]   = std::move(task_t([i] {
        apricot::interruption_point();
        return i * i;
      }));

    } else {
      tasks[i]   = std::move(task_t([i] { return i * i; }));
    }
    results[i] = tasks[i].get_future();
  }

  for (int i = 0; i < num_tasks; ++i) {
    task_queue.enqueue(std::move(tasks[i]));
  }

  std::cout << "IThread squares\n";
  for (auto& res: results) {
    using namespace std::chrono_literals;
    auto status = res.wait_for(1000ms);
    if (status == std::future_status::ready) {
      std::cout << res.get() << '\n';
    } else {
      std::cout << "... future not ready\n";
    }
  }
  std::cout << "==============\n";

  for (auto& consumer: consumers) {
    consumer.join();
  }

  return 0;
}