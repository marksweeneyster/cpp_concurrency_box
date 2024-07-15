#ifndef CPP_CONCURRENCY_BOX_INCLUDE_APRICOT_SHARED_MTX_HPP
#define CPP_CONCURRENCY_BOX_INCLUDE_APRICOT_SHARED_MTX_HPP

#include <shared_mutex>
#include <thread>

namespace apricot {

  // This requires a common `shared_mutex` for reads and writes.
  // Multiple reads can happen concurrently and use a `shared_lock`.
  // A write operation uses the usual `lock_guard`.
  // Any running read operation will block a write operation.
  // A running write will block any pending reads.
  class MultiRead {
  public:
    explicit MultiRead(int d) : data(d) {}

    int read() const {
      std::shared_lock<std::shared_mutex> lk(mtx);
      // introduce some latency
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(10ms);
      return data;
    }

    void write(int d) {
      std::lock_guard<std::shared_mutex> lk(mtx);
      data = d;
    }

  private:
    int data;
    mutable std::shared_mutex mtx;
  };
}// namespace apricot
#endif//CPP_CONCURRENCY_BOX_INCLUDE_APRICOT_SHARED_MTX_HPP
