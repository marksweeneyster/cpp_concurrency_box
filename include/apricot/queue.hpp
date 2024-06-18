#ifndef CONCURRENCY_BOX_EXERCISE_INCLUDE_APRICOT_QUEUE_HPP
#define CONCURRENCY_BOX_EXERCISE_INCLUDE_APRICOT_QUEUE_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <utility>

namespace apricot {

  /*
   * Queue0 uses a single mutex and std::queue for the FIFO
   */
  template<typename T>
  class Queue0 {
  public:

    void enqueue(T data) {
      std::lock_guard<std::mutex> lk(data_mutex);
      // try to avoid copy ctor calls when T is a non-trivial type
      data_queue.push(std::move(data));
      data_cv.notify_one();
    }

    void dequeue(T& data) {
      std::unique_lock<std::mutex> lk(data_mutex);
      data_cv.wait(lk, [this] { return !data_queue.empty(); });
      // try to avoid copy ctor calls when T is a non-trivial type
      data = std::move(data_queue.front());
      data_queue.pop();
    }

    bool empty() const {
      std::lock_guard<std::mutex> lk(data_mutex);
      return data_queue.empty();
    }

    Queue0()                         = default;
    Queue0(const Queue0&)            = delete;
    Queue0& operator=(const Queue0&) = delete;

    ~Queue0() = default;

  private:
    std::queue<T> data_queue;
    mutable std::mutex data_mutex;
    std::condition_variable data_cv;
  };

  /*
   * Queue1 uses separate mutexes for enqueuing and dequeuing.
   * FIFO order is maintained by a linked list.
   */
  template<typename T>
  class Queue1 {
  public:
    void enqueue(T data) {
      auto data_ptr       = std::make_unique<T>(std::move(data));
      auto node_ptr       = std::make_unique<Node>();
      const auto new_tail = node_ptr.get();

      std::lock_guard<std::mutex> tail_lock(tail_mutex);
      tail->data = std::move(data_ptr);
      tail->next = std::move(node_ptr);
      tail       = new_tail;
    }

    void dequeue(T& data) {
      NodePtr old_head = pop_head();
      while (!old_head) {
        if (is_aborted()) {
          return;
        }
        old_head = pop_head();
      }
      data = std::move(*(old_head->data));
    }

    std::optional<T> dequeue() {
      NodePtr old_head = pop_head();
      while (!old_head) {
        if (is_aborted()) {
          return {};
        }
        old_head = pop_head();
      }
      return std::move(*(old_head->data));
    }

    bool empty() const {
      std::lock_guard<std::mutex> head_lock(head_mutex);
      return head.get() == get_tail();
    }

    void set_aborted() {
      std::scoped_lock guard(head_mutex, tail_mutex);
      aborted = true;
    }
    bool is_aborted() const { return aborted; }

    // Adding a dummy node partially simplifies the mutex management.
    // The enqueue only needs to lock the tail mutex.
    Queue1() : head(new Node), tail(head.get()) {}

    Queue1(const Queue1&)            = delete;
    Queue1& operator=(const Queue1&) = delete;

    ~Queue1() = default;

  private:
    using DataPtr = std::unique_ptr<T>;
    struct Node {
      DataPtr data;
      std::unique_ptr<Node> next;

      Node() : data(nullptr), next(nullptr) {}
    };
    using NodePtr = std::unique_ptr<Node>;

    NodePtr head;
    Node* tail;

    mutable std::mutex head_mutex;
    mutable std::mutex tail_mutex;

    std::atomic<bool> aborted = false;

    Node* get_tail() const {
      std::lock_guard<std::mutex> tail_lock(tail_mutex);
      return tail;
    }

    /**
     * pop_head will simultaneously lock both head and tail mutexes.
     * The tail lock should be very brief.
     */
    std::unique_ptr<Node> pop_head() {
      std::lock_guard<std::mutex> head_lock(head_mutex);
      if (head.get() == get_tail()) {
        // This is the dummy node, so the queue is empty
        return nullptr;
      }
      std::unique_ptr<Node> old_head = std::move(head);
      head                           = std::move(old_head->next);
      return old_head;
    }
  };

}// namespace apricot

#endif//CONCURRENCY_BOX_EXERCISE_INCLUDE_APRICOT_QUEUE_HPP
