// https://en.cppreference.com/w/cpp/thread/jthread/request_stop.html

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

using namespace std::chrono_literals;

// Helper function to quickly show which thread printed what
void print(auto txt) { std::cout << std::this_thread::get_id() << ' ' << txt; }

int main() {
  // A sleepy worker thread
  std::jthread sleepy_worker([](const std::stop_token& stoken) {
    for (int i = 10; i; --i) {
      std::this_thread::sleep_for(199ms);
      if (stoken.stop_requested()) {
        print("Sleepy worker is requested to stop\n");
        return;
      }
      print("Sleepy worker goes back to sleep\n");
    }
  });

  // A waiting worker thread
  // The condition variable will be awoken by the stop request.
  std::jthread waiting_worker([](std::stop_token stoken) {
    std::mutex mutex;
    std::unique_lock lock(mutex);
    std::condition_variable_any().wait(lock, std::move(stoken),
                                       [] { return false; });
    print("Waiting worker is requested to stop\n");
    return;
  });

  // Sleep this thread to give threads time to spin
  std::this_thread::sleep_for(400ms);

  // std::jthread::request_stop() can be called explicitly:
  print("Requesting stop of sleepy worker\n");
  sleepy_worker.request_stop();
  sleepy_worker.join();
  print("Sleepy worker joined\n"); // main exits right after this prints

  // Or automatically using RAII:
  // waiting_worker's destructor will call request_stop()
  // and join the thread automatically.
}
