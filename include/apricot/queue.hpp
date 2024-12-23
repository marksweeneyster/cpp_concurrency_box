#ifndef CONCURRENCY_BOX_EXERCISE_INCLUDE_APRICOT_QUEUE_HPP
#define CONCURRENCY_BOX_EXERCISE_INCLUDE_APRICOT_QUEUE_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

namespace apricot {

  /*
   * Queue0 uses a single mutex and std::queue for the FIFO
   */
  template<typename T>
  class Queue0 {
  public:
    void enqueue(T data) {
      std::lock_guard<std::mutex> lk(data_mutex);
      data_queue.push(std::move(data));
      data_cv.notify_one();
    }

    void enqueue(std::vector<T> data_vec) {
      std::lock_guard<std::mutex> lk(data_mutex);

      for (auto& data: data_vec) {
        data_queue.push(std::move(data));
      }

      data_cv.notify_one();
    }

    /*
     * Thread safe pop. Users should a sentinel value for the input/output and
     * check if that value has changed.
     * @data : reference value for the popped data
     */
    bool dequeue(T& data) {
      std::unique_lock<std::mutex> lk(data_mutex);
      data_cv.wait_for(lk, data_wait_ms,
                       [this] { return !data_queue.empty(); });
      if (!data_queue.empty()) {
        data = std::move(data_queue.front());
        data_queue.pop();
        return true;
      }
      return false;
    }

    bool empty() const {
      std::lock_guard<std::mutex> lk(data_mutex);
      return data_queue.empty();
    }

    void clear() {
      std::lock_guard<std::mutex> lk(data_mutex);

      while (!data_queue.empty()) {
        data_queue.pop();
      }
    }

    /*
     * Constructor for single mutex queue.
     * @timeout_ms : the maximum time in milliseconds that dequeue will wait
     */
    explicit Queue0(int timeout_ms) : data_wait_ms(timeout_ms) {}
    Queue0(const Queue0&)            = delete;
    Queue0& operator=(const Queue0&) = delete;

    Queue0(Queue0&& other) noexcept : data_wait_ms(other.data_wait_ms) {
      std::lock_guard<std::mutex> lk(other.data_mutex);
      data_queue = std::move(other.data_queue);
    }

    Queue0& operator=(Queue0&& other) noexcept {
      if (this != &other) {
        data_wait_ms = other.data_wait_ms;
        std::scoped_lock lock(data_mutex, other.data_mutex);

        if (data_queue.empty()) {
          data_queue = std::move(other.data_queue);
        } else {
          while (!other.data_queue.empty()) {
            //T tmp = std::move(other.data_queue.front());
            data_queue.push(std::move(other.data_queue.front()));
            other.data_queue.pop();
          }
        }
      }
      return *this;
    }
    ~Queue0() = default;

  private:
    std::queue<T> data_queue;
    mutable std::mutex data_mutex;
    std::condition_variable data_cv;
    std::chrono::milliseconds data_wait_ms;
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

  template<typename T>
  class Queue2 {
  public:
    /*
     * This will return as soon as the head_mutex is free.
     * If the queue is non-empty then the value will be updated and the queue is
     * popped. Otherwise, the value is not modified.
     *
     * @value : output parameter
     * @return : true if the value was updated from the queue.
     */
    bool try_pop(T& value) { return try_pop_head(value); }

    /*
     * This will wait until the queue is non-empty or until the wait-for time 
     * has elapsed on the condition_variable.
     * If it is a timeout then the value is not modified.
     *
     * @value : output parameter
     * @return : true if the value was updated from the queue.
     */
    bool wait_and_pop(T& value) { return wait_pop_head(value); }


    void push(T data) {
      auto data_ptr       = std::make_unique<T>(std::move(data));
      auto node_ptr       = std::make_unique<Node>();
      const auto new_tail = node_ptr.get();
      {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        tail->data = std::move(data_ptr);
        tail->next = std::move(node_ptr);
        tail       = new_tail;
      }
      data_cond.notify_one();
    }

    /*
     * Push a vector of values, order will be preserved in the queue.
     *
     * @data_vec : input
     */
    void push(std::vector<T> data_vec) {
      {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        for (auto& data: data_vec) {
          auto data_ptr       = std::make_unique<T>(std::move(data));
          auto node_ptr       = std::make_unique<Node>();
          const auto new_tail = node_ptr.get();
          tail->data = std::move(data_ptr);
          tail->next = std::move(node_ptr);
          tail       = new_tail;
        }
      }
      data_cond.notify_one();
    }

    bool empty() const {
      std::lock_guard<std::mutex> head_lock(head_mutex);
      return head.get() == get_tail();
    }

    explicit Queue2(int timeout_ms)
        : head(new Node), tail(head.get()), data_wait_ms(timeout_ms) {}

    Queue2(const Queue2&)            = delete;
    Queue2& operator=(const Queue2&) = delete;

    ~Queue2() = default;

    Queue2(Queue2&& other) noexcept : data_wait_ms(other.data_wait_ms) {
      std::scoped_lock lock(other.head_mutex, other.tail_mutex);
      head       = std::move(other.head);
      tail       = other.tail;
      other.head = std::make_unique<Node>();
      other.tail = other.head.get();
    }

    Queue2& operator=(Queue2&& other) noexcept {
      if (this != &other) {
        data_wait_ms = other.data_wait_ms;
        std::scoped_lock lock(head_mutex, tail_mutex, other.head_mutex,
                              other.tail_mutex);

        bool other_empty = other.head.get() == other.tail;
        bool this_empty  = head.get() == tail;

        if (!other_empty) {
          if (this_empty) {
            head = std::move(other.head);
            tail = other.tail;
          } else {
            while (other.head.get() != other.tail) {
              auto data_ptr = std::make_unique<T>(std::move(*other.head->data));
              other.pop_head();

              auto node_ptr       = std::make_unique<Node>();
              const auto new_tail = node_ptr.get();

              tail->data = std::move(data_ptr);
              tail->next = std::move(node_ptr);
              tail       = new_tail;
            }
          }
          other.head = std::make_unique<Node>();
          other.tail = other.head.get();
        }
      }
      return *this;
    }

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

    std::condition_variable data_cond;
    std::chrono::milliseconds data_wait_ms;

    Node* get_tail() const {
      std::lock_guard<std::mutex> tail_lock(tail_mutex);
      return tail;
    }

    void pop_head() {
      std::unique_ptr<Node> old_head = std::move(head);
      head                           = std::move(old_head->next);
    }

    /*
     * @is_empty : output parameter, false if queue is not empty
     * returns (moves) the lock on the head_mutex
     */
    std::unique_lock<std::mutex> wait_for_data(bool& is_empty) {
      std::unique_lock<std::mutex> head_lock(head_mutex);
      data_cond.wait_for(head_lock, data_wait_ms, [&] {
        is_empty = head.get() == get_tail();
        return !is_empty;
      });

      // gcc warns of bypassing copy-elision
      // return std::move(head_lock);
      return head_lock;
    }

    // Returns true if value has been updated
    bool wait_pop_head(T& value) {
      bool is_empty = true;// empty queue
      std::unique_lock<std::mutex> head_lock(std::move(wait_for_data(is_empty)));
      if (!is_empty) {
        value = std::move(*head->data);
        pop_head();
        return true;
      }
      return false;
    }

    // Returns true if value has been updated
    bool try_pop_head(T& value) {
      std::lock_guard<std::mutex> head_lock(head_mutex);
      if (head.get() == get_tail()) {
        return false;
      }
      value = std::move(*head->data);
      pop_head();
      return true;
    }
  };

// Cleaned up version of Queue2
  template<typename T>
  class Queue3 {
  public:
    /*
     * This will return as soon as the head_mutex is free.
     * If the queue is non-empty then the value will be updated and the queue is
     * popped. Otherwise, the value is not modified.
     *
     * @value : output parameter
     * @return : true if the value was updated from the queue.
     */
    bool try_pop(T& value) {
      std::lock_guard<std::mutex> head_lock(head_mutex);
      if (head.get() == get_tail()) {
        return false;
      }
      value = std::move(*head->data);
      pop_head();
      return true;
    }

    /*
     * This will wait until the queue is non-empty or until the wait-for time
     * has elapsed on the condition_variable.
     * If it is a timeout then the value is not modified.
     *
     * @value : output parameter
     * @return : true if the value was updated from the queue.
     */
    bool wait_and_pop(T& value) {
      bool is_empty = true;// empty queue

      std::unique_lock<std::mutex> head_lock(head_mutex);
      data_cond.wait_for(head_lock, data_wait_ms, [&] {
        is_empty = head.get() == get_tail();
        return !is_empty;
      });

      if (!is_empty) {
        value = std::move(*head->data);
        pop_head();
        return true;
      }

      return false;
    }

    void push(T data) {
      auto data_ptr       = std::make_unique<T>(std::move(data));
      auto node_ptr       = std::make_unique<Node>();
      const auto new_tail = node_ptr.get();
      {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        tail->data = std::move(data_ptr);
        tail->next = std::move(node_ptr);
        tail       = new_tail;
      }
      data_cond.notify_one();
    }

    /*
     * Push a vector of values, order will be preserved in the queue.
     *
     * @data_vec : input
     */
    void push(std::vector<T> data_vec) {
      {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        for (auto& data: data_vec) {
          auto data_ptr       = std::make_unique<T>(std::move(data));
          auto node_ptr       = std::make_unique<Node>();
          const auto new_tail = node_ptr.get();
          tail->data = std::move(data_ptr);
          tail->next = std::move(node_ptr);
          tail       = new_tail;
        }
      }
      data_cond.notify_one();
    }

    bool empty() const {
      std::lock_guard<std::mutex> head_lock(head_mutex);
      return head.get() == get_tail();
    }

    explicit Queue3(int timeout_ms)
        : head(new Node), tail(head.get()), data_wait_ms(timeout_ms) {}

    Queue3(const Queue3&)            = delete;
    Queue3& operator=(const Queue3&) = delete;

    ~Queue3() = default;

    Queue3(Queue3&& other) noexcept : data_wait_ms(other.data_wait_ms) {
      std::scoped_lock lock(other.head_mutex, other.tail_mutex);
      head       = std::move(other.head);
      tail       = other.tail;
      other.head = std::make_unique<Node>();
      other.tail = other.head.get();
    }

    Queue3& operator=(Queue3&& other) noexcept {
      if (this != &other) {
        data_wait_ms = other.data_wait_ms;
        std::scoped_lock lock(head_mutex, tail_mutex, other.head_mutex,
                              other.tail_mutex);

        bool other_empty = other.head.get() == other.tail;
        bool this_empty  = head.get() == tail;

        if (!other_empty) {
          if (this_empty) {
            head = std::move(other.head);
            tail = other.tail;
          } else {
            while (other.head.get() != other.tail) {
              auto data_ptr = std::make_unique<T>(std::move(*other.head->data));
              other.pop_head();

              auto node_ptr       = std::make_unique<Node>();
              const auto new_tail = node_ptr.get();

              tail->data = std::move(data_ptr);
              tail->next = std::move(node_ptr);
              tail       = new_tail;
            }
          }
          other.head = std::make_unique<Node>();
          other.tail = other.head.get();
        }
      }
      return *this;
    }

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

    std::condition_variable data_cond;
    std::chrono::milliseconds data_wait_ms;

    Node* get_tail() const {
      std::lock_guard<std::mutex> tail_lock(tail_mutex);
      return tail;
    }

    void pop_head() {
      std::unique_ptr<Node> old_head = std::move(head);
      head                           = std::move(old_head->next);
    }
  };


}// namespace apricot

#endif//CONCURRENCY_BOX_EXERCISE_INCLUDE_APRICOT_QUEUE_HPP
