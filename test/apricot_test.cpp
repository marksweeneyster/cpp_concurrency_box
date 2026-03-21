#include "apricot/queue.hpp"
#include <gtest/gtest.h>

struct NoCopy {
  int data;

  explicit NoCopy(int d) : data(d) {}

  NoCopy(const NoCopy&)                  = delete;
  NoCopy& operator=(const NoCopy& other) = delete;

  NoCopy(NoCopy&&)            = default;
  NoCopy& operator=(NoCopy&&) = default;
  ~NoCopy()                   = default;
};

TEST(Queue0, BasicQueue) {
  apricot::queue0_t<int> i_queue(100);

  EXPECT_TRUE(i_queue.empty());

  i_queue.enqueue(11);
  EXPECT_FALSE(i_queue.empty());

  i_queue.enqueue(12);
  i_queue.enqueue(13);
  i_queue.enqueue(14);

  int data = 0;
  i_queue.dequeue(data);
  EXPECT_EQ(data, 11);
  i_queue.dequeue(data);
  EXPECT_EQ(data, 12);
  i_queue.dequeue(data);
  EXPECT_EQ(data, 13);
  i_queue.dequeue(data);
  EXPECT_EQ(data, 14);

  EXPECT_TRUE(i_queue.empty());
}

TEST(Queue1, BasicQueue) {
  apricot::queue1_t<int> i_queue;

  EXPECT_TRUE(i_queue.empty());

  i_queue.enqueue(11);
  EXPECT_FALSE(i_queue.empty());

  i_queue.enqueue(12);
  i_queue.enqueue(13);
  i_queue.enqueue(14);

  int data = 0;
  i_queue.dequeue(data);
  EXPECT_EQ(data, 11);
  i_queue.dequeue(data);
  EXPECT_EQ(data, 12);
  i_queue.dequeue(data);
  EXPECT_EQ(data, 13);
  i_queue.dequeue(data);
  EXPECT_EQ(data, 14);

  EXPECT_TRUE(i_queue.empty());
}

TEST(Queue0, NoCopy) {
  apricot::queue0_t<NoCopy> nc_queue(100);

  nc_queue.enqueue(NoCopy(1));
  nc_queue.enqueue(NoCopy(2));
  nc_queue.enqueue(NoCopy(3));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);

  nc_queue.dequeue(nc1);
  nc_queue.dequeue(nc2);
  nc_queue.dequeue(nc3);

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);
}

TEST(Queue0, BulkPush) {
  std::vector<NoCopy> vec;
  vec.emplace_back(1);
  vec.emplace_back(2);
  vec.emplace_back(3);
  vec.emplace_back(4);

  apricot::queue0_t<NoCopy> queue(100);

  // This should fail to compile
  //queue.enqueue(vec);

  queue.enqueue(std::move(vec));

  // This should fail to compile
  //std::map<NoCopy, NoCopy> v = {};
  //queue.enqueue2(std::move(v));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);
  NoCopy nc4(0);

  queue.dequeue(nc1);
  queue.dequeue(nc2);
  queue.dequeue(nc3);
  queue.dequeue(nc4);

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);
  EXPECT_EQ(nc4.data, 4);

  NoCopy parr[4] = {NoCopy(5), NoCopy(6), NoCopy(7), NoCopy(8)};
  std::span<NoCopy> nc_span(parr);
  queue.enqueue(std::move(nc_span));// NOLINT(*-move-const-arg)

  queue.dequeue(nc1);
  queue.dequeue(nc2);
  queue.dequeue(nc3);
  queue.dequeue(nc4);

  EXPECT_EQ(nc1.data, 5);
  EXPECT_EQ(nc2.data, 6);
  EXPECT_EQ(nc3.data, 7);
  EXPECT_EQ(nc4.data, 8);

  EXPECT_TRUE(queue.empty());
  std::array<NoCopy, 0> empty_array{};
  queue.enqueue(std::move(empty_array));// NOLINT(*-move-const-arg)
  EXPECT_TRUE(queue.empty());
}

TEST(Queue0, thread) {
  apricot::queue0_t<NoCopy> queue(1000);
  int max_val = 100;

  std::thread listener([&queue, max_val]() {
    NoCopy nc(0);

    for (int ii = 1; ii <= max_val; ++ii) {
      nc.data = 0;
      while (!queue.dequeue(nc)) {}
      EXPECT_EQ(nc.data, ii);
    }
    return;
  });
  listener.detach();

  //  for (int ii = 1; ii <= max_val; ++ii) {
  //    queue.enqueue(NoCopy(ii));
  //  }

  std::vector<NoCopy> vec;
  vec.reserve(max_val);
  for (int ii = 1; ii <= max_val; ++ii) {
    vec.emplace_back(ii);
  }
  queue.enqueue(std::move(vec));

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_TRUE(queue.empty());
}

TEST(Queue0, Move) {
  apricot::queue0_t<NoCopy> queue0(100);
  apricot::queue0_t<NoCopy> queue1(100);

  queue0.enqueue(NoCopy(1));
  queue0.enqueue(NoCopy(2));
  queue0.enqueue(NoCopy(3));

  queue1.enqueue(NoCopy(4));
  queue1.enqueue(NoCopy(5));
  queue1.enqueue(NoCopy(6));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);

  queue0 = std::move(queue1);

  EXPECT_TRUE(queue1.empty());

  queue0.dequeue(nc1);
  queue0.dequeue(nc2);
  queue0.dequeue(nc3);

  EXPECT_TRUE(queue0.empty());

  EXPECT_EQ(nc1.data, 4);
  EXPECT_EQ(nc2.data, 5);
  EXPECT_EQ(nc3.data, 6);

  queue0.enqueue(NoCopy(7));
  queue0.enqueue(NoCopy(8));
  queue0.enqueue(NoCopy(9));

  apricot::queue0_t<NoCopy> queue2(std::move(queue0));

  EXPECT_TRUE(queue0.empty());
  EXPECT_FALSE(queue2.empty());

  queue2.dequeue(nc1);
  queue2.dequeue(nc2);
  queue2.dequeue(nc3);

  EXPECT_EQ(nc1.data, 7);
  EXPECT_EQ(nc2.data, 8);
  EXPECT_EQ(nc3.data, 9);
}

TEST(Queue0, Move2) {
  apricot::queue0_t<NoCopy> queue0(100);
  apricot::queue0_t<NoCopy> queue1(100);

  queue0.enqueue(NoCopy(1));
  queue0.enqueue(NoCopy(2));
  queue0.enqueue(NoCopy(3));

  queue1.enqueue(NoCopy(4));
  queue1.enqueue(NoCopy(5));
  queue1.enqueue(NoCopy(6));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);
  NoCopy nc4(0);
  NoCopy nc5(0);
  NoCopy nc6(0);

  queue0 += std::move(queue1);

  EXPECT_TRUE(queue1.empty());

  queue0.dequeue(nc1);
  queue0.dequeue(nc2);
  queue0.dequeue(nc3);
  queue0.dequeue(nc4);
  queue0.dequeue(nc5);
  queue0.dequeue(nc6);

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);
  EXPECT_EQ(nc4.data, 4);
  EXPECT_EQ(nc5.data, 5);
  EXPECT_EQ(nc6.data, 6);
}

TEST(Queue1, NoCopy) {
  apricot::queue1_t<NoCopy> nc_queue;

  nc_queue.enqueue(NoCopy(1));
  nc_queue.enqueue(NoCopy(2));
  nc_queue.enqueue(NoCopy(3));

  auto nc_opt1 = nc_queue.dequeue();
  auto nc_opt2 = nc_queue.dequeue();
  auto nc_opt3 = nc_queue.dequeue();

  ASSERT_TRUE(nc_opt1);
  ASSERT_TRUE(nc_opt2.has_value());
  ASSERT_TRUE(nc_opt3.has_value());

  EXPECT_EQ(nc_opt1.value().data, 1);
  EXPECT_EQ(nc_opt2.value().data, 2);
  EXPECT_EQ(nc_opt3.value().data, 3);
}

TEST(Queue1, Abort) {
  apricot::queue1_t<double> d_queue;

  // add a blocked consumer
  std::thread consumer([&d_queue] {
    auto d_opt = d_queue.dequeue();
    ASSERT_FALSE(d_opt.has_value());
  });

  d_queue.set_aborted();

  consumer.join();
  ASSERT_TRUE(d_queue.empty());
}

TEST(Queue2, WaitAndPop) {
  apricot::queue2_t<NoCopy> nc_queue(100);

  nc_queue.push(NoCopy(1));
  nc_queue.push(NoCopy(2));
  nc_queue.push(NoCopy(3));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);

  EXPECT_TRUE(nc_queue.wait_and_pop(nc1));
  EXPECT_TRUE(nc_queue.wait_and_pop(nc2));
  EXPECT_TRUE(nc_queue.wait_and_pop(nc3));

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);

  EXPECT_TRUE(nc_queue.empty());
  nc1.data = 42;
  EXPECT_FALSE(nc_queue.wait_and_pop(nc1));
  EXPECT_EQ(nc1.data, 42);
}

TEST(Queue2, TryPop) {
  apricot::queue2_t<NoCopy> nc_queue(0);

  nc_queue.push(NoCopy(1));
  nc_queue.push(NoCopy(2));
  nc_queue.push(NoCopy(3));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);

  EXPECT_TRUE(nc_queue.try_pop(nc1));
  EXPECT_TRUE(nc_queue.try_pop(nc2));
  EXPECT_TRUE(nc_queue.try_pop(nc3));

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);

  EXPECT_TRUE(nc_queue.empty());
  nc1.data = 42;
  EXPECT_FALSE(nc_queue.try_pop(nc1));
  EXPECT_EQ(nc1.data, 42);
}

TEST(Queue2, Vector) {
  std::vector<NoCopy> vec;
  vec.reserve(4);
  vec.emplace_back(1);
  vec.emplace_back(2);
  vec.emplace_back(3);
  vec.emplace_back(4);

  apricot::queue2_t<NoCopy> queue(100);

  queue.push(std::move(vec));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);
  NoCopy nc4(0);

  queue.try_pop(nc1);
  queue.try_pop(nc2);
  queue.try_pop(nc3);
  queue.try_pop(nc4);

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);
  EXPECT_EQ(nc4.data, 4);
}

TEST(Queue2, thread) {
  apricot::queue2_t<NoCopy> queue(1000);
  int max_val = 100;

  std::thread listener([&queue, max_val]() {
    NoCopy nc(0);

    for (int ii = 1; ii <= max_val; ++ii) {
      nc.data = 0;
      while (!queue.wait_and_pop(nc)) {}
      EXPECT_EQ(nc.data, ii);
    }
    return;
  });
  listener.detach();

  for (int ii = 1; ii <= max_val; ++ii) {
    queue.push(NoCopy(ii));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_TRUE(queue.empty());
}

TEST(Queue2, Move) {
  apricot::queue2_t<NoCopy> queue0(100);
  apricot::queue2_t<NoCopy> queue1(100);

  queue0.push(NoCopy(1));
  queue0.push(NoCopy(2));
  queue0.push(NoCopy(3));

  queue1.push(NoCopy(4));
  queue1.push(NoCopy(5));
  queue1.push(NoCopy(6));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);
  NoCopy nc4(0);
  NoCopy nc5(0);
  NoCopy nc6(0);

  queue0 = std::move(queue1);

  EXPECT_TRUE(queue1.empty());

  queue0.try_pop(nc1);
  queue0.try_pop(nc2);
  queue0.try_pop(nc3);
  queue0.try_pop(nc4);
  queue0.try_pop(nc5);
  queue0.try_pop(nc6);

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);
  EXPECT_EQ(nc4.data, 4);
  EXPECT_EQ(nc5.data, 5);
  EXPECT_EQ(nc6.data, 6);

  queue0.push(NoCopy(7));
  queue0.push(NoCopy(8));
  queue0.push(NoCopy(9));

  apricot::queue2_t<NoCopy> queue2(std::move(queue0));

  EXPECT_TRUE(queue0.empty());
  EXPECT_FALSE(queue2.empty());

  queue2.try_pop(nc1);
  queue2.try_pop(nc2);
  queue2.try_pop(nc3);

  EXPECT_EQ(nc1.data, 7);
  EXPECT_EQ(nc2.data, 8);
  EXPECT_EQ(nc3.data, 9);
}

TEST(Queue3, WaitAndPop) {
  apricot::queue3_t<NoCopy> nc_queue(100);

  nc_queue.push(NoCopy(1));
  nc_queue.push(NoCopy(2));
  nc_queue.push(NoCopy(3));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);

  EXPECT_TRUE(nc_queue.wait_and_pop(nc1));
  EXPECT_TRUE(nc_queue.wait_and_pop(nc2));
  EXPECT_TRUE(nc_queue.wait_and_pop(nc3));

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);

  EXPECT_TRUE(nc_queue.empty());
  nc1.data = 42;
  EXPECT_FALSE(nc_queue.wait_and_pop(nc1));
  EXPECT_EQ(nc1.data, 42);
}

TEST(Queue3, TryPop) {
  apricot::queue3_t<NoCopy> nc_queue(0);

  nc_queue.push(NoCopy(1));
  nc_queue.push(NoCopy(2));
  nc_queue.push(NoCopy(3));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);

  EXPECT_TRUE(nc_queue.try_pop(nc1));
  EXPECT_TRUE(nc_queue.try_pop(nc2));
  EXPECT_TRUE(nc_queue.try_pop(nc3));

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);

  EXPECT_TRUE(nc_queue.empty());
  nc1.data = 42;
  EXPECT_FALSE(nc_queue.try_pop(nc1));
  EXPECT_EQ(nc1.data, 42);
}

TEST(Queue3, Vector) {
  std::vector<NoCopy> vec;
  vec.emplace_back(1);
  vec.emplace_back(2);
  vec.emplace_back(3);
  vec.emplace_back(4);

  apricot::queue3_t<NoCopy> queue(100);

  queue.push(std::move(vec));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);
  NoCopy nc4(0);

  queue.try_pop(nc1);
  queue.try_pop(nc2);
  queue.try_pop(nc3);
  queue.try_pop(nc4);

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);
  EXPECT_EQ(nc4.data, 4);
}

TEST(Queue3, clear_queue) {
  apricot::queue3_t<NoCopy> queue(1000);
  int max_val = 100;

  for (int ii = 1; ii <= max_val; ++ii) {
    queue.push(NoCopy(ii));
  }
  EXPECT_FALSE(queue.empty());
  EXPECT_NO_THROW(queue.clear());
  EXPECT_TRUE(queue.empty());
}

TEST(Queue3, thread) {
  apricot::queue3_t<NoCopy> queue(1000);
  int max_val = 100;

  std::thread listener([&queue, max_val]() {
    NoCopy nc(0);

    for (int ii = 1; ii <= max_val; ++ii) {
      nc.data = 0;
      while (!queue.wait_and_pop(nc)) {}
      EXPECT_EQ(nc.data, ii);
    }
    return;
  });
  listener.detach();

  for (int ii = 1; ii <= max_val; ++ii) {
    queue.push(NoCopy(ii));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_TRUE(queue.empty());
}

TEST(Queue3, Move) {
  apricot::queue3_t<NoCopy> queue0(100);
  apricot::queue3_t<NoCopy> queue1(100);

  queue0.push(NoCopy(1));
  queue0.push(NoCopy(2));
  queue0.push(NoCopy(3));

  queue1.push(NoCopy(4));
  queue1.push(NoCopy(5));
  queue1.push(NoCopy(6));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);
  NoCopy nc4(0);
  NoCopy nc5(0);
  NoCopy nc6(0);

  queue0 = std::move(queue1);

  EXPECT_TRUE(queue1.empty());

  queue0.try_pop(nc1);
  queue0.try_pop(nc2);
  queue0.try_pop(nc3);
  queue0.try_pop(nc4);
  queue0.try_pop(nc5);
  queue0.try_pop(nc6);

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);
  EXPECT_EQ(nc4.data, 4);
  EXPECT_EQ(nc5.data, 5);
  EXPECT_EQ(nc6.data, 6);

  queue0.push(NoCopy(7));
  queue0.push(NoCopy(8));
  queue0.push(NoCopy(9));

  apricot::queue3_t<NoCopy> queue2(std::move(queue0));

  EXPECT_TRUE(queue0.empty());
  EXPECT_FALSE(queue2.empty());

  queue2.try_pop(nc1);
  queue2.try_pop(nc2);
  queue2.try_pop(nc3);

  EXPECT_EQ(nc1.data, 7);
  EXPECT_EQ(nc2.data, 8);
  EXPECT_EQ(nc3.data, 9);
}

TEST(ts_queue2, WaitForPop) {
  other::ts_queue2_t<NoCopy> nc_queue;

  nc_queue.push(NoCopy(1));
  nc_queue.push(NoCopy(2));
  nc_queue.push(NoCopy(3));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);

  auto wait_time = std::chrono::milliseconds(10);
  EXPECT_TRUE(nc_queue.wait_for_pop(nc1, wait_time));
  EXPECT_TRUE(nc_queue.wait_for_pop(nc2, wait_time));
  EXPECT_TRUE(nc_queue.wait_for_pop(nc3, wait_time));

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);

  EXPECT_TRUE(nc_queue.empty());
  nc1.data = 42;
  EXPECT_FALSE(nc_queue.wait_for_pop(nc1, wait_time));
  EXPECT_EQ(nc1.data, 42);
}

TEST(ts_queue2, TryPop) {
  other::ts_queue2_t<NoCopy> nc_queue;

  nc_queue.push(NoCopy(1));
  nc_queue.push(NoCopy(2));
  nc_queue.push(NoCopy(3));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);

  EXPECT_TRUE(nc_queue.try_pop(nc1));
  EXPECT_TRUE(nc_queue.try_pop(nc2));
  EXPECT_TRUE(nc_queue.try_pop(nc3));

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);

  EXPECT_TRUE(nc_queue.empty());
  nc1.data = 42;
  EXPECT_FALSE(nc_queue.try_pop(nc1));
  EXPECT_EQ(nc1.data, 42);
}

TEST(ts_queue2, BulkPush) {
  std::vector<NoCopy> vec;
  vec.emplace_back(1);
  vec.emplace_back(2);
  vec.emplace_back(3);
  vec.emplace_back(4);

  other::ts_queue2_t<NoCopy> queue;

  queue.push(std::move(vec));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);
  NoCopy nc4(0);

  queue.try_pop(nc1);
  queue.try_pop(nc2);
  queue.try_pop(nc3);
  queue.try_pop(nc4);

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);
  EXPECT_EQ(nc4.data, 4);
}

TEST(ts_queue2, clear_queue) {
  other::ts_queue2_t<NoCopy> queue;
  int max_val = 100;

  for (int ii = 1; ii <= max_val; ++ii) {
    queue.push(NoCopy(ii));
  }
  EXPECT_FALSE(queue.empty());
  EXPECT_NO_THROW(queue.clear());
  EXPECT_TRUE(queue.empty());
}

TEST(ts_queue2, thread) {
  other::ts_queue2_t<NoCopy> queue;
  int max_val = 100;

  std::thread listener([&queue, max_val]() {
    NoCopy nc(0);

    for (int ii = 1; ii <= max_val; ++ii) {
      nc.data = 0;
      EXPECT_TRUE(queue.wait_pop(nc));
      EXPECT_EQ(nc.data, ii);
    }
    return;
  });
  listener.detach();

  for (int ii = 1; ii <= max_val; ++ii) {
    queue.push(NoCopy(ii));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_TRUE(queue.empty());
}

TEST(ts_queue2, Move) {
  other::ts_queue2_t<NoCopy> queue0;
  other::ts_queue2_t<NoCopy> queue1;

  queue0.push(NoCopy(1));
  queue0.push(NoCopy(2));
  queue0.push(NoCopy(3));

  queue1.push(NoCopy(4));
  queue1.push(NoCopy(5));
  queue1.push(NoCopy(6));

  NoCopy nc1(0);
  NoCopy nc2(0);
  NoCopy nc3(0);
  NoCopy nc4(0);
  NoCopy nc5(0);
  NoCopy nc6(0);

  queue0 += std::move(queue1);

  EXPECT_TRUE(queue1.empty());

  queue0.try_pop(nc1);
  queue0.try_pop(nc2);
  queue0.try_pop(nc3);
  queue0.try_pop(nc4);
  queue0.try_pop(nc5);
  queue0.try_pop(nc6);

  EXPECT_EQ(nc1.data, 1);
  EXPECT_EQ(nc2.data, 2);
  EXPECT_EQ(nc3.data, 3);
  EXPECT_EQ(nc4.data, 4);
  EXPECT_EQ(nc5.data, 5);
  EXPECT_EQ(nc6.data, 6);

  queue0.push(NoCopy(7));
  queue0.push(NoCopy(8));
  queue0.push(NoCopy(9));

  other::ts_queue2_t<NoCopy> queue2(std::move(queue0));

  EXPECT_TRUE(queue0.empty());
  EXPECT_FALSE(queue2.empty());

  queue2.try_pop(nc1);
  queue2.try_pop(nc2);
  queue2.try_pop(nc3);

  EXPECT_EQ(nc1.data, 7);
  EXPECT_EQ(nc2.data, 8);
  EXPECT_EQ(nc3.data, 9);
}
