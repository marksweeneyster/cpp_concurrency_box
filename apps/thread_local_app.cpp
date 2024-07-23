#include <iostream>
#include <mutex>
#include <string>
#include <string_view>

class X {
  inline static thread_local std::string s="X::";
  mutable std::mutex mtx;

public:
  explicit X(std::string_view sv)  {
    X::s += sv;
  }

  void append(std::string_view sv) const {
    X::s += sv;
  }

  void print() const {
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << X::s << '\n';
  }
};

int main() {
  // main thread
  X x("x::");

  // worker thread 1
  auto lam1 = [&x](){
    x.append("ONE::");
    x.append("one\n");
    x.print();
  };

  // worker thread 2
  auto lam2 = [&x](){
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