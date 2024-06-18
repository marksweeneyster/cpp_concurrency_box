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

  // Queue of tasks that return integers
  using FT_Queue = apricot::Queue1<std::future<int>>;

  FT_Queue ft_queue;

  std::vector<int> squares(num_tasks, -7);

  // setup several consumer threads waiting for enqueued elements
  std::vector<std::thread> consumers(num_tasks);
  size_t indx = 0;
  for (auto& consumer: consumers) {
    int& rtn_val = squares[indx++];
    consumer     = std::thread([&ft_queue, &rtn_val] {
      auto fut_opt = ft_queue.dequeue();
      if (fut_opt) {
        rtn_val = fut_opt->get();
      } else {
        // This should only happen if set_aborted has been called.
        rtn_val = -1;
      }
    });
  }

  std::vector<std::thread> producers(num_tasks);
  for (int i = 0; i < num_tasks; ++i) {
    producers[i] = std::thread([i, &ft_queue] {
      ft_queue.enqueue(std::async(std::launch::async, [i] { return i * i; }));
    });
  }

  for (auto& producer: producers) {
    producer.join();
  }

  for (auto& consumer: consumers) {
    consumer.join();
  }

  std::cout << "Queue1 squares\n";
  std::cout << "==============\n";
  for (const auto& square: squares) {
    std::cout << square << '\n';
  }
  std::cout << "==============\n";

  return 0;
}
