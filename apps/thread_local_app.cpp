#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

/**
 * storage specifiers: https://en.cppreference.com/w/cpp/language/storage_duration
 *
 * "The thread_local keyword is only allowed for objects declared at namespace
 *   scope, objects declared at block scope, and static data members.
 *
 *  It indicates that the object has thread storage duration.
 *
 *  If thread_local is the only storage class specifier applied to a
 *   block scope variable, static is also implied."
 *
 *
 * "thread storage duration: The storage for the object is allocated when the
 *   thread begins and deallocated when the thread ends.
 *
 *  Each thread has its own instance of the object.
 *
 *  Only objects declared thread_local have this storage duration.
 *
 *  thread_local can appear together with static or extern to adjust linkage."
 */

namespace {
  thread_local std::string ss = "foo ";
}

class X {
  inline static thread_local std::string s = "X::";
  mutable std::mutex mtx;

public:
  explicit X(std::string_view sv) { X::s += sv; }

  void append(std::string_view sv) const { X::s += sv; ss += sv; }

  void print() const {
    // mutex is just to prevent stdout getting garbled from different threads
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << X::s << '\t' << ss << '\n';
  }
};

int main() {
  // main thread
  ss = "goo ";

  X x("x::");

  // worker thread 1
  auto lam1 = [&x]() {
    x.append("ONE::");
    x.append("one\n");
    x.print();
  };

  // worker thread 2
  auto lam2 = [&x]() {
    x.append("TWO::");
    x.append("two::");
    x.append("2\n");
    x.print();
  };

  std::thread t1(lam1);
  std::thread t2(lam2);

  t1.join();
  t2.join();

  x.print();
}