#include "atomics/lock_free_stack.h"

int main() {
  totally_atomic::lock_free_stack<int> lock_free_stack;
  lock_free_stack.push(49);
  lock_free_stack.push(48);
  lock_free_stack.push(47);
  lock_free_stack.push(46);
  lock_free_stack.push(45);
  lock_free_stack.push(42);
  auto val = lock_free_stack.pop();

  while (lock_free_stack.pop()) {}

  return val ? *val : -1;
}
