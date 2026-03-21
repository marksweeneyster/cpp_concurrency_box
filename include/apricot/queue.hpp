#ifndef CONCURRENCY_BOX_EXERCISE_INCLUDE_APRICOT_QUEUE_HPP
#define CONCURRENCY_BOX_EXERCISE_INCLUDE_APRICOT_QUEUE_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

namespace apricot {

  template<typename R>
  concept Iterable = requires(R range) {
    { range.begin() } -> std::input_iterator;
    { range.end() } -> std::sentinel_for<decltype(range.begin())>;
    { range.empty() } -> std::same_as<bool>;
  };

  /*
   * queue0_t uses a single mutex and std::queue for the FIFO
   */
  template<typename T>
  class queue0_t {
  public:
    void enqueue(T data) {
      std::lock_guard<std::mutex> lock(_data_mutex);
      _data_queue.push(std::move(data));
      _data_cv.notify_one();
    }

    void enqueue0(auto&& data_range) {
      using el_type = decltype(*data_range.begin());
      static_assert(std::is_same_v<T, std::decay_t<el_type>>,
                    "data_range elements must be "
                    "<T>");

      if (data_range.empty()) {
        return;
      }

      std::lock_guard<std::mutex> lock(_data_mutex);
      for (auto& data: data_range) {
        _data_queue.push(std::move(data));
      }

      _data_cv.notify_one();
    }
    // above signature with auto&& implicitly allows L-values
    void enqueue0(auto& data_range) = delete;

    template<Iterable R>
      requires requires(R r, T t) { t = std::move(*r.begin()); }
    void enqueue2(R&& range) {
      if (range.empty()) {
        return;
      }

      std::lock_guard<std::mutex> lock(_data_mutex);
      for (auto& data: range) {
        _data_queue.push(std::move(data));
      }

      _data_cv.notify_one();
    }

    template<Iterable R>
      requires requires(R::value_type r, T t) { t = std::move(r); }
    void enqueue(R&& range) {
      if (range.empty()) {
        return;
      }

      std::lock_guard<std::mutex> lock(_data_mutex);
      for (auto& data: range) {
        _data_queue.push(std::move(data));
      }

      _data_cv.notify_one();
    }
    // above enqueue signature with auto&& implicitly allows L-values
    template<Iterable R>
    void enqueue(R& data_range) = delete;

    /*
     * Thread safe pop. Users should set a sentinel value for the input/output and
     * check if that value has changed.
     * @data : reference value for the popped data
     */
    bool dequeue(T& data) {
      std::unique_lock<std::mutex> lock(_data_mutex);
      _data_cv.wait_for(lock, _data_wait_ms,
                        [this] { return !_data_queue.empty(); });
      if (!_data_queue.empty()) {
        data = std::move(_data_queue.front());
        _data_queue.pop();
        return true;
      }
      return false;
    }

    bool empty() const {
      std::lock_guard<std::mutex> lock(_data_mutex);
      return _data_queue.empty();
    }

    void clear() {
      std::lock_guard<std::mutex> lock(_data_mutex);

      while (!_data_queue.empty()) {
        _data_queue.pop();
      }
    }

    /*
     * Constructor for single mutex queue.
     * @timeout_ms : the maximum time in milliseconds that dequeue will wait
     */
    explicit queue0_t(int timeout_ms) : _data_wait_ms(timeout_ms) {}
    queue0_t(const queue0_t&)            = delete;
    queue0_t& operator=(const queue0_t&) = delete;

    queue0_t(queue0_t&& other) noexcept : _data_wait_ms(other._data_wait_ms) {
      std::lock_guard<std::mutex> lock(other._data_mutex);
      _data_queue = std::move(other._data_queue);
    }

    queue0_t& operator=(queue0_t&& other) noexcept {
      if (this != &other) {
        _data_wait_ms = other._data_wait_ms;
        std::scoped_lock lock(_data_mutex, other._data_mutex);
        while (!_data_queue.empty()) {
          _data_queue.pop();
        }
        _data_queue = std::move(other._data_queue);
      }
      return *this;
    }

    queue0_t& operator+=(queue0_t&& other) noexcept {
      if (this != &other) {
        _data_wait_ms = other._data_wait_ms;
        std::scoped_lock lock(_data_mutex, other._data_mutex);

        if (_data_queue.empty()) {
          _data_queue = std::move(other._data_queue);
        } else {
          while (!other._data_queue.empty()) {
            //T tmp = std::move(other.data_queue.front());
            _data_queue.push(std::move(other._data_queue.front()));
            other._data_queue.pop();
          }
        }
      }
      return *this;
    }

    ~queue0_t() = default;

  private:
    std::queue<T> _data_queue;
    mutable std::mutex _data_mutex;
    std::condition_variable _data_cv;
    std::chrono::milliseconds _data_wait_ms;
  };

  /*
   * queue1_t uses separate mutexes for enqueuing and dequeuing.
   * FIFO order is maintained by a linked list.
   */
  template<typename T>
  class queue1_t {
  public:
    void enqueue(T data) {
      auto data_ptr       = std::make_unique<T>(std::move(data));
      auto node_ptr       = std::make_unique<node_t>();
      const auto new_tail = node_ptr.get();

      std::lock_guard<std::mutex> tail_lock(_tail_mutex);
      _tail->_data = std::move(data_ptr);
      _tail->_next = std::move(node_ptr);
      _tail        = new_tail;
    }

    void dequeue(T& data) {
      NodePtr old_head = pop_head();
      while (!old_head) {
        if (is_aborted()) {
          return;
        }
        old_head = pop_head();
      }
      data = std::move(*(old_head->_data));
    }

    std::optional<T> dequeue() {
      NodePtr old_head = pop_head();
      while (!old_head) {
        if (is_aborted()) {
          return {};
        }
        old_head = pop_head();
      }
      return std::move(*(old_head->_data));
    }

    bool empty() const {
      std::lock_guard<std::mutex> head_lock(_head_mutex);
      return _head.get() == get_tail();
    }

    void set_aborted() {
      std::scoped_lock guard(_head_mutex, _tail_mutex);
      _aborted = true;
    }
    bool is_aborted() const { return _aborted; }

    // Adding a dummy node partially simplifies the mutex management.
    // The enqueue only needs to lock the tail mutex.
    queue1_t() : _head(new node_t), _tail(_head.get()) {}

    queue1_t(const queue1_t&)            = delete;
    queue1_t& operator=(const queue1_t&) = delete;

    ~queue1_t() = default;

  private:
    using DataPtr = std::unique_ptr<T>;
    struct node_t {
      DataPtr _data;
      std::unique_ptr<node_t> _next;

      node_t() : _data(nullptr), _next(nullptr) {}
    };
    using NodePtr = std::unique_ptr<node_t>;

    NodePtr _head;
    node_t* _tail;

    mutable std::mutex _head_mutex;
    mutable std::mutex _tail_mutex;

    std::atomic<bool> _aborted = false;

    node_t* get_tail() const {
      std::lock_guard<std::mutex> tail_lock(_tail_mutex);
      return _tail;
    }

    /**
     * pop_head will simultaneously lock both head and tail mutexes.
     * The tail lock should be very brief.
     */
    std::unique_ptr<node_t> pop_head() {
      std::lock_guard<std::mutex> head_lock(_head_mutex);
      if (_head.get() == get_tail()) {
        // This is the dummy node, so the queue is empty
        return nullptr;
      }
      std::unique_ptr<node_t> old_head = std::move(_head);
      _head                            = std::move(old_head->_next);
      return old_head;
    }
  };

  template<typename T>
  class queue2_t {
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
      auto node_ptr       = std::make_unique<node_t>();
      const auto new_tail = node_ptr.get();
      {
        std::lock_guard<std::mutex> tail_lock(_tail_mutex);
        _tail->_data = std::move(data_ptr);
        _tail->_next = std::move(node_ptr);
        _tail        = new_tail;
      }
      _data_cond.notify_one();
    }

    /*
     * Push a vector of values, order will be preserved in the queue.
     *
     * @data_vec : input
     */
    void push(std::vector<T> data_vec) {
      {
        std::lock_guard<std::mutex> tail_lock(_tail_mutex);
        for (auto& data: data_vec) {
          auto data_ptr       = std::make_unique<T>(std::move(data));
          auto node_ptr       = std::make_unique<node_t>();
          const auto new_tail = node_ptr.get();
          _tail->_data        = std::move(data_ptr);
          _tail->_next        = std::move(node_ptr);
          _tail               = new_tail;
        }
      }
      _data_cond.notify_one();
    }

    bool empty() const {
      std::lock_guard<std::mutex> head_lock(_head_mutex);
      return _head.get() == get_tail();
    }

    explicit queue2_t(int timeout_ms)
        : _head(new node_t), _tail(_head.get()), _data_wait_ms(timeout_ms) {}

    queue2_t(const queue2_t&)            = delete;
    queue2_t& operator=(const queue2_t&) = delete;

    ~queue2_t() = default;

    queue2_t(queue2_t&& other) noexcept : _data_wait_ms(other._data_wait_ms) {
      std::scoped_lock lock(other._head_mutex, other._tail_mutex);
      _head       = std::move(other._head);
      _tail       = other._tail;
      other._head = std::make_unique<node_t>();
      other._tail = other._head.get();
    }

    queue2_t& operator=(queue2_t&& other) noexcept {
      if (this != &other) {
        _data_wait_ms = other._data_wait_ms;
        std::scoped_lock lock(_head_mutex, _tail_mutex, other._head_mutex,
                              other._tail_mutex);

        bool other_empty = other._head.get() == other._tail;
        bool this_empty  = _head.get() == _tail;

        if (!other_empty) {
          if (this_empty) {
            _head = std::move(other._head);
            _tail = other._tail;
          } else {
            while (other._head.get() != other._tail) {
              auto data_ptr =
                      std::make_unique<T>(std::move(*other._head->_data));
              other.pop_head();

              auto node_ptr       = std::make_unique<node_t>();
              const auto new_tail = node_ptr.get();

              _tail->_data = std::move(data_ptr);
              _tail->_next = std::move(node_ptr);
              _tail        = new_tail;
            }
          }
          other._head = std::make_unique<node_t>();
          other._tail = other._head.get();
        }
      }
      return *this;
    }

  private:
    using DataPtr = std::unique_ptr<T>;
    struct node_t {
      DataPtr _data;
      std::unique_ptr<node_t> _next;

      node_t() : _data(nullptr), _next(nullptr) {}
    };
    using NodePtr = std::unique_ptr<node_t>;

    NodePtr _head;
    node_t* _tail;

    mutable std::mutex _head_mutex;
    mutable std::mutex _tail_mutex;

    std::condition_variable _data_cond;
    std::chrono::milliseconds _data_wait_ms;

    node_t* get_tail() const {
      std::lock_guard<std::mutex> tail_lock(_tail_mutex);
      return _tail;
    }

    void pop_head() {
      std::unique_ptr<node_t> old_head = std::move(_head);
      _head                            = std::move(old_head->_next);
    }

    /*
     * @is_empty : output parameter, false if queue is not empty
     * returns (moves) the lock on the head_mutex
     */
    std::unique_lock<std::mutex> wait_for_data(bool& is_empty) {
      std::unique_lock<std::mutex> head_lock(_head_mutex);
      _data_cond.wait_for(head_lock, _data_wait_ms, [&] {
        is_empty = _head.get() == get_tail();
        return !is_empty;
      });

      // gcc warns of bypassing copy-elision
      // return std::move(head_lock);
      return head_lock;
    }

    // Returns true if value has been updated
    bool wait_pop_head(T& value) {
      bool is_empty = true;// empty queue
      std::unique_lock<std::mutex> head_lock(wait_for_data(is_empty));

      if (!is_empty) {
        value = std::move(*_head->_data);
        pop_head();
        return true;
      }
      return false;
    }

    // Returns true if value has been updated
    bool try_pop_head(T& value) {
      std::lock_guard<std::mutex> head_lock(_head_mutex);
      if (_head.get() == get_tail()) {
        return false;
      }
      value = std::move(*_head->_data);
      pop_head();
      return true;
    }
  };

  // Cleaned up version of queue2_t
  template<typename T>
  class queue3_t {
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
      std::lock_guard<std::mutex> head_lock(_head_mutex);
      if (_head.get() == get_tail()) {
        return false;
      }
      value = std::move(*_head->_data);
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

      // Using the helper function "wait_for_data" thread-sanitizer (plus asan and usan) give a clean bill of health

      // If I move the body of "wait_for_data" here then thread-sanitizer complains (warns) about:
      // 1. double lock of a mutex (.build/apps/demo3_tsan+0x9e80) in __gthread_mutex_lock(pthread_mutex_t*)a
      // 2. data race (.build/apps/demo3_tsan+0x117f5) in std::__uniq_ptr_impl<apricot::queue3_t<std::future<int> >::Node, std::default_delete<apricot::queue3_t<std::future<int> >::Node> >::_M_ptr() const
      std::unique_lock<std::mutex> head_lock(wait_for_data(is_empty));

      if (!is_empty) {
        value = std::move(*_head->_data);
        pop_head();
        return true;
      }

      return false;
    }

    void push(T data) {
      auto data_ptr       = std::make_unique<T>(std::move(data));
      auto node_ptr       = std::make_unique<node_t>();
      const auto new_tail = node_ptr.get();
      {
        std::lock_guard<std::mutex> tail_lock(_tail_mutex);
        _tail->_data = std::move(data_ptr);
        _tail->_next = std::move(node_ptr);
        _tail        = new_tail;
      }
      _data_cond.notify_one();
    }

    /*
     * Push a vector of values, order will be preserved in the queue.
     *
     * @data_vec : input
     */
    void push(std::vector<T> data_vec) {
      {
        std::lock_guard<std::mutex> tail_lock(_tail_mutex);
        for (auto& data: data_vec) {
          auto data_ptr       = std::make_unique<T>(std::move(data));
          auto node_ptr       = std::make_unique<node_t>();
          const auto new_tail = node_ptr.get();
          _tail->_data        = std::move(data_ptr);
          _tail->_next        = std::move(node_ptr);
          _tail               = new_tail;
        }
      }
      _data_cond.notify_one();
    }

    bool empty() const {
      std::lock_guard<std::mutex> head_lock(_head_mutex);
      return _head.get() == get_tail();
    }

    void clear() {
      std::scoped_lock lock(_head_mutex, _tail_mutex);
      while (_head.get() != _tail) {
        pop_head();
      }
    }

    explicit queue3_t(int timeout_ms)
        : _head(new node_t), _tail(_head.get()), _data_wait_ms(timeout_ms) {}

    queue3_t(const queue3_t&)            = delete;
    queue3_t& operator=(const queue3_t&) = delete;

    ~queue3_t() = default;

    queue3_t(queue3_t&& other) noexcept : _data_wait_ms(other._data_wait_ms) {
      std::scoped_lock lock(other._head_mutex, other._tail_mutex);
      _head       = std::move(other._head);
      _tail       = other._tail;
      other._head = std::make_unique<node_t>();
      other._tail = other._head.get();
    }

    queue3_t& operator=(queue3_t&& other) noexcept {
      if (this != &other) {
        _data_wait_ms = other._data_wait_ms;
        std::scoped_lock lock(_head_mutex, _tail_mutex, other._head_mutex,
                              other._tail_mutex);

        bool other_empty = other._head.get() == other._tail;
        bool this_empty  = _head.get() == _tail;

        if (!other_empty) {
          if (this_empty) {
            _head = std::move(other._head);
            _tail = other._tail;
          } else {
            while (other._head.get() != other._tail) {
              auto data_ptr =
                      std::make_unique<T>(std::move(*other._head->_data));
              other.pop_head();

              auto node_ptr       = std::make_unique<node_t>();
              const auto new_tail = node_ptr.get();

              _tail->_data = std::move(data_ptr);
              _tail->_next = std::move(node_ptr);
              _tail        = new_tail;
            }
          }
          other._head = std::make_unique<node_t>();
          other._tail = other._head.get();
        }
      }
      return *this;
    }

  private:
    using DataPtr = std::unique_ptr<T>;
    struct node_t {
      DataPtr _data;
      std::unique_ptr<node_t> _next;

      node_t() : _data(nullptr), _next(nullptr) {}
    };
    using NodePtr = std::unique_ptr<node_t>;

    NodePtr _head;
    node_t* _tail;

    mutable std::mutex _head_mutex;
    mutable std::mutex _tail_mutex;

    std::condition_variable _data_cond;
    std::chrono::milliseconds _data_wait_ms;

    node_t* get_tail() const {
      std::lock_guard<std::mutex> tail_lock(_tail_mutex);
      return _tail;
    }

    void pop_head() {
      std::unique_ptr<node_t> old_head = std::move(_head);
      _head                            = std::move(old_head->_next);
    }

    std::unique_lock<std::mutex> wait_for_data(bool& is_empty) {
      std::unique_lock<std::mutex> head_lock(_head_mutex);
      _data_cond.wait_for(head_lock, _data_wait_ms, [&] {
        is_empty = _head.get() == get_tail();
        return !is_empty;
      });

      return head_lock;
    }
  };


}// namespace apricot

namespace other {
  template<typename T>
  class ts_queue_t {
  public:
    void push(T data) {
      std::lock_guard<std::mutex> lock(_data_mutex);
      _data_queue.push(std::move(data));
      _data_cv.notify_one();
    }

    void push(std::vector<T> data_vec) {
      std::lock_guard<std::mutex> lock(_data_mutex);
      for (auto& data: data_vec) {
        _data_queue.push(std::move(data));
      }

      _data_cv.notify_one();
    }

    bool wait_for_pop(T& data, std::chrono::milliseconds wait_time) {
      std::unique_lock<std::mutex> lock(_data_mutex);
      _data_cv.wait_for(lock, wait_time,
                        [this] { return !_data_queue.empty(); });
      if (!_data_queue.empty()) {
        data = std::move(_data_queue.front());
        _data_queue.pop();
        return true;
      }
      return false;
    }

    bool wait_pop(T& data) {
      std::unique_lock<std::mutex> lock(_data_mutex);
      _data_cv.wait(lock, [this] { return !_data_queue.empty(); });
      if (!_data_queue.empty()) {
        data = std::move(_data_queue.front());
        _data_queue.pop();
        return true;
      }
      return false;
    }

    bool try_pop(T& data) {
      std::unique_lock<std::mutex> lock(_data_mutex);
      if (!_data_queue.empty()) {
        data = std::move(_data_queue.front());
        _data_queue.pop();
        return true;
      }
      return false;
    }

    bool empty() const {
      std::lock_guard<std::mutex> lock(_data_mutex);
      return _data_queue.empty();
    }

    void clear() {
      std::lock_guard<std::mutex> lock(_data_mutex);

      while (!_data_queue.empty()) {
        _data_queue.pop();
      }
    }

    ts_queue_t()                             = default;
    ts_queue_t(const ts_queue_t&)            = delete;
    ts_queue_t& operator=(const ts_queue_t&) = delete;

    ts_queue_t(ts_queue_t&& other) noexcept {
      std::lock_guard<std::mutex> lock(other._data_mutex);
      _data_queue = std::move(other._data_queue);
    }

    ts_queue_t& operator=(ts_queue_t&& other) noexcept {
      if (this != &other) {
        std::scoped_lock lock(_data_mutex, other._data_mutex);
        while (!_data_queue.empty()) {
          _data_queue.pop();
        }
        _data_queue = std::move(other._data_queue);
      }
      return *this;
    }

    ts_queue_t& operator+=(ts_queue_t&& other) noexcept {
      if (this != &other) {
        std::scoped_lock lock(_data_mutex, other._data_mutex);
        if (_data_queue.empty()) {
          if (!other._data_queue.empty()) {
            _data_queue = std::move(other._data_queue);
          }
        } else {
          while (!other._data_queue.empty()) {
            T tmp = std::move(other._data_queue.front());
            _data_queue.push(std::move(tmp));
            other._data_queue.pop();
          }
        }
      }
      return *this;
    }

    ~ts_queue_t() = default;

  private:
    std::queue<T> _data_queue;
    mutable std::mutex _data_mutex;
    std::condition_variable _data_cv;
  };


  template<typename T>
  class ts_queue2_t {
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
      std::lock_guard<std::mutex> head_lock(_head_mutex);
      if (_head.get() == get_tail()) {
        return false;
      }
      value = std::move(*_head->_data);
      pop_head();
      return true;
    }

    /*
   * This will wait until the queue is non-empty
   *
   * @value : output parameter
   * @return : true if the value was updated from the queue.
   */
    bool wait_pop(T& value) {
      bool is_empty = true;// empty queue

      // Using the helper function "wait_pop" thread-sanitizer (plus asan and usan) give a clean bill of health

      // If I move the body of "wait_data" here then thread-sanitizer complains (warns) about:
      // 1. double lock of a mutex (.build/apps/demo3_tsan+0x9e80) in __gthread_mutex_lock(pthread_mutex_t*)a
      // 2. data race (.build/apps/demo3_tsan+0x117f5) in std::__uniq_ptr_impl<apricot::ts_queue2_t<std::future<int>
      // >::Node, std::default_delete<apricot::ts_queue2_t<std::future<int> >::Node> >::_M_ptr() const
      std::unique_lock<std::mutex> head_lock(wait_data(is_empty));

      if (!is_empty) {
        value = std::move(*_head->_data);
        pop_head();
        return true;
      }

      return false;
    }

    bool wait_for_pop(T& value, std::chrono::milliseconds wait_time) {
      bool is_empty = true;// empty queue
      std::unique_lock<std::mutex> head_lock(
              wait_for_data(is_empty, wait_time));

      if (!is_empty) {
        value = std::move(*_head->_data);
        pop_head();
        return true;
      }

      return false;
    }

    void push(T data) {
      auto data_ptr       = std::make_unique<T>(std::move(data));
      auto node_ptr       = std::make_unique<node_t>();
      const auto new_tail = node_ptr.get();
      {
        std::lock_guard<std::mutex> tail_lock(_tail_mutex);
        _tail->_data = std::move(data_ptr);
        _tail->_next = std::move(node_ptr);
        _tail        = new_tail;
      }
      _data_cond.notify_one();
    }

    /*
     * Push a range of values, order will be preserved in the queue.
     * For thread safety the caller should std::move the container in the
     * function call.
     *
     * @range : input
     */
    template<apricot::Iterable R>
      requires requires(R::value_type r, T t) { t = std::move(r); }
    void push(R&& range) {
      if (range.empty()) {
        return;
      }
      {
        std::lock_guard<std::mutex> tail_lock(_tail_mutex);
        for (auto& data: range) {
          auto data_ptr       = std::make_unique<T>(std::move(data));
          auto node_ptr       = std::make_unique<node_t>();
          const auto new_tail = node_ptr.get();
          _tail->_data        = std::move(data_ptr);
          _tail->_next        = std::move(node_ptr);
          _tail               = new_tail;
        }
      }
      _data_cond.notify_one();
    }
    // above push signature with auto&& implicitly allows L-values
    template<apricot::Iterable R>
    void push(R& data_range) = delete;

    bool empty() const {
      std::lock_guard<std::mutex> head_lock(_head_mutex);
      return _head.get() == get_tail();
    }

    void clear() {
      std::scoped_lock lock(_head_mutex, _tail_mutex);
      while (_head.get() != _tail) {
        pop_head();
      }
    }

    ts_queue2_t() : _head(new node_t), _tail(_head.get()) {}

    ts_queue2_t(const ts_queue2_t&)            = delete;
    ts_queue2_t& operator=(const ts_queue2_t&) = delete;

    ~ts_queue2_t() = default;

    ts_queue2_t(ts_queue2_t&& other) noexcept {
      std::scoped_lock lock(other._head_mutex, other._tail_mutex);
      _head       = std::move(other._head);
      _tail       = other._tail;
      other._head = std::make_unique<node_t>();
      other._tail = other._head.get();
    }

    ts_queue2_t& operator=(ts_queue2_t&& other) noexcept {
      if (this != &other) {
        std::scoped_lock lock(_head_mutex, _tail_mutex, other._head_mutex,
                              other._tail_mutex);
        while (_head.get() != _tail) {
          pop_head();
        }
        _head       = std::move(other._head);
        _tail       = other._tail;
        other._head = std::make_unique<node_t>();
        other._tail = other._head.get();
      }
      return *this;
    }

    ts_queue2_t& operator+=(ts_queue2_t&& other) noexcept {
      if (this != &other) {
        std::scoped_lock lock(_head_mutex, _tail_mutex, other._head_mutex,
                              other._tail_mutex);

        bool other_empty = other._head.get() == other._tail;
        bool this_empty  = _head.get() == _tail;

        if (!other_empty) {
          if (this_empty) {
            _head = std::move(other._head);
            _tail = other._tail;
          } else {
            while (other._head.get() != other._tail) {
              auto data_ptr =
                      std::make_unique<T>(std::move(*other._head->_data));
              other.pop_head();

              auto node_ptr       = std::make_unique<node_t>();
              const auto new_tail = node_ptr.get();

              _tail->_data = std::move(data_ptr);
              _tail->_next = std::move(node_ptr);
              _tail        = new_tail;
            }
          }
          other._head = std::make_unique<node_t>();
          other._tail = other._head.get();
        }
      }
      return *this;
    }

  private:
    using DataPtr = std::unique_ptr<T>;
    struct node_t {
      DataPtr _data;
      std::unique_ptr<node_t> _next;

      node_t() : _data(nullptr), _next(nullptr) {}
    };
    using NodePtr = std::unique_ptr<node_t>;

    NodePtr _head;
    node_t* _tail;

    mutable std::mutex _head_mutex;
    mutable std::mutex _tail_mutex;

    std::condition_variable _data_cond;

    node_t* get_tail() const {
      std::lock_guard<std::mutex> tail_lock(_tail_mutex);
      return _tail;
    }

    void pop_head() {
      std::unique_ptr<node_t> old_head = std::move(_head);
      _head                            = std::move(old_head->_next);
    }

    std::unique_lock<std::mutex> wait_data(bool& is_empty) {
      std::unique_lock<std::mutex> head_lock(_head_mutex);
      _data_cond.wait(head_lock, [&] {
        is_empty = _head.get() == get_tail();
        return !is_empty;
      });

      return head_lock;
    }

    std::unique_lock<std::mutex>
    wait_for_data(bool& is_empty, std::chrono::milliseconds wait_time) {
      std::unique_lock<std::mutex> head_lock(_head_mutex);
      _data_cond.wait_for(head_lock, wait_time, [&] {
        is_empty = _head.get() == get_tail();
        return !is_empty;
      });

      return head_lock;
    }
  };
}// namespace other
#endif//CONCURRENCY_BOX_EXERCISE_INCLUDE_APRICOT_QUEUE_HPP
