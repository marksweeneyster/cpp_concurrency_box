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
  apricot::Queue0<int> i_queue;

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
  apricot::Queue1<int> i_queue;

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
  apricot::Queue0<NoCopy> nc_queue;

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

TEST(Queue1, NoCopy) {
  apricot::Queue1<NoCopy> nc_queue;

  nc_queue.enqueue(NoCopy(1));
  nc_queue.enqueue(NoCopy(2));
  nc_queue.enqueue(NoCopy(3));

  auto nc_opt1 = nc_queue.dequeue();
  auto nc_opt2 = nc_queue.dequeue();
  auto nc_opt3 = nc_queue.dequeue();

  ASSERT_TRUE(nc_opt1.has_value());
  ASSERT_TRUE(nc_opt2.has_value());
  ASSERT_TRUE(nc_opt3.has_value());

  EXPECT_EQ(nc_opt1.value().data, 1);
  EXPECT_EQ(nc_opt2.value().data, 2);
  EXPECT_EQ(nc_opt3.value().data, 3);
}

TEST(Queue1, Abort) {
  apricot::Queue1<double> d_queue;

  // add a blocked consumer
  std::thread consumer([&d_queue] {
    auto d_opt = d_queue.dequeue();
    ASSERT_FALSE(d_opt.has_value());
  });

  d_queue.set_aborted();

  consumer.join();
  ASSERT_TRUE(d_queue.empty());
}
