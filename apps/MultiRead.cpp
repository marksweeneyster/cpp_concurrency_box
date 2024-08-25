#include "apricot/shared_mtx.hpp"
#include <future>
#include <iostream>
#include <vector>

int main(int argc, char* argv[]) {
  uint16_t num_tasks = 111;

  apricot::MultiRead object(42);

  auto mid = num_tasks / 2;

  using read_t  = std::future<int>;
  using write_t = std::future<void>;

  std::vector<read_t> readers(num_tasks - 1);
  write_t writer;

  uint16_t jj = 0;
  for (uint16_t ii = 0; ii < num_tasks; ++ii) {
    if (ii == mid) {
      writer = std::async(&apricot::MultiRead::write, &object, 13);
    } else {
      readers[jj++] = std::async(&apricot::MultiRead::read, &object);
    }
  }

  jj = 0;
  for (uint16_t ii = 0; ii < num_tasks; ++ii) {
    std::cout << ii << " ";
    if (ii == mid) {
      writer.get();
      std::cout << "write";
    } else {
      auto val = readers[jj++].get();
      std::cout << val;
    }
    std::cout << '\n';
  }

  return 0;
}