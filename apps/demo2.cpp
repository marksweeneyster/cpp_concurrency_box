#include "apricot/queue.hpp"
#include <future>
#include <iostream>

int main(int argc, char* argv[]) {
  uint16_t num_tasks = 11;

  if (argc > 1) {
    num_tasks = static_cast<uint16_t>(std::strtol(argv[1], nullptr, 10));
  } else {
    std::cout << "Using the default number of tasks (" << num_tasks
              << "), add a command line integer argument to change.\n\n";
  }

  using task_t   = std::packaged_task<int()>;
  using future_t = std::future<int>;

  apricot::Queue1<task_t> task_queue;

  // setup several consumer threads waiting for enqueued elements
  std::vector<std::thread> consumers(num_tasks);

  for (auto& consumer: consumers) {
    consumer = std::thread([&task_queue] {
      auto task_opt = task_queue.dequeue();
      if (task_opt) {
        auto& task = task_opt.value();
        // do the work
        task();
      }
    });
  }

  // now, define the work to be done
  std::vector<task_t> tasks(num_tasks);
  std::vector<future_t> results(num_tasks);

  for (int i = 0; i < num_tasks; ++i) {
    tasks[i]   = std::move(task_t([i] { return i * i; }));
    results[i] = tasks[i].get_future();
  }

  std::vector<std::thread> producers(num_tasks);
  for (int i = 0; i < num_tasks; ++i) {
    producers[i] = std::thread([i, &task_queue, &tasks] {
      task_queue.enqueue(std::move(tasks[i]));
    });
  }

  std::cout << "PT squares\n";
  for (auto& res: results) {
    using namespace std::chrono_literals;
    auto status = res.wait_for(10ms);
    if (status == std::future_status::ready) {
      std::cout << res.get() << '\n';
    } else {
      std::cout << "... future not ready\n";
    }
  }
  std::cout << "==============\n";

  for (auto& producer: producers) {
    producer.join();
  }

  for (auto& consumer: consumers) {
    consumer.join();
  }

  return 0;
}
