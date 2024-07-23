#ifndef CPP_CONCURRENCY_BOX_INCLUDE_APRICOT_INT_THREAD_HPP
#define CPP_CONCURRENCY_BOX_INCLUDE_APRICOT_INT_THREAD_HPP

#include <future>
#include <iostream>
#include <queue>
#include <thread>

// TODO: Work in progress ...
namespace apricot {

  /**
   * This is an attempt to get an interruptible thread in C++ 17, similar to the
   * C++ 20 std::jthread in that it could be interrupted if it's stuck waiting
   * and we want to join and exit (as in a shutdown event).
   *
   * It seems to run OK but thread sanitizers generate several warnings.
   * This implementation is based on "C++ Concurrency in Action", section 9.2
   *
   * While it's currently not so "smart" the name is an analogy to smart pointers.
   */

  class InterruptFlag {
    std::atomic<bool> flag;

  public:
    InterruptFlag() : flag(false) {}

    void set() { flag.store(true, std::memory_order_relaxed); }
    bool is_set() const { return flag.load(std::memory_order_relaxed); }
  };

  /*
   * (quote)
   * This thread_local flag is the primary reason you can’t use plain
   * std::thread to manage the thread; it needs to be allocated in a way
   * that the interruptible_thread instance can access, as well
   * as the newly started thread.
   */
  thread_local InterruptFlag this_thread_interrupt_flag;

  struct thread_interrupted : public std::exception {};

  void interruption_point() {
    if (this_thread_interrupt_flag.is_set()) {
      throw thread_interrupted();
    }
  }

  class SmartThread {
    std::thread internal_thread;
    InterruptFlag* flag;

  public:
    SmartThread() = default;

    template<typename FnType>
    explicit SmartThread(FnType fn) {
      std::promise<InterruptFlag*> p;
      internal_thread = std::thread([fn, &p] {
        p.set_value(&this_thread_interrupt_flag);
        try {
          fn();
        } catch (thread_interrupted& e) {
          std::cerr << std::this_thread::get_id() << " interrupted\n";
        }
      });

      flag = p.get_future().get();
    }

    void interrupt() {
      if (flag) {
        flag->set();
      }
    }

    void join() { internal_thread.join(); }

    bool joinable() const { return internal_thread.joinable(); }

    void detach() { internal_thread.detach(); }

    std::thread::id get_id() const { return internal_thread.get_id(); }
  };

}// namespace apricot

#endif//CPP_CONCURRENCY_BOX_INCLUDE_APRICOT_INT_THREAD_HPP
