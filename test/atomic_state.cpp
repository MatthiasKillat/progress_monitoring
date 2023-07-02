#include <gtest/gtest.h>

#include "state/atomic_state.hpp"

#include <iostream>
#include <thread>

struct Foo {

  Foo() = default;

  Foo(int x, int y) : x(x), y(y) {}

  int x{73};
  char space[1024];
  int y{21};

  // ~Foo() { std::cout << "~Foo: " << x << " " << y << std::endl; }
};

bool operator==(const Foo &lhs, const Foo &rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool operator!=(const Foo &lhs, const Foo &rhs) { return !(lhs == rhs); }

namespace traits {
template <> struct is_memcopyable<Foo> : public std::true_type {};
} // namespace traits

namespace {

using Sut = sw_atomic_state<Foo>;

class AtomicStateTest : public ::testing::Test {
protected:
  virtual void SetUp() {}

  virtual void TearDown() {}

  Sut sut;
};

TEST_F(AtomicStateTest, default_constructed) {
  EXPECT_EQ(sut.get(), Foo());
  EXPECT_EQ(sut.count(), 0);
}

TEST_F(AtomicStateTest, non_default_constructed) {
  Sut s(42, 43);
  EXPECT_EQ(s.get(), Foo(42, 43));
  EXPECT_EQ(sut.count(), 0);
}

TEST_F(AtomicStateTest, set_value) {
  sut.set(66, 6);
  EXPECT_EQ(sut.get(), Foo(66, 6));
}

TEST_F(AtomicStateTest, load_value) {
  sut.set(37, 12);
  auto foo = sut.load();
  EXPECT_EQ(sut.get(), Foo(37, 12));
}

TEST_F(AtomicStateTest, set_increases_counter) {
  auto prev = sut.count();
  sut.set(1, 1);
  EXPECT_GE(sut.count(), prev);
}

TEST_F(AtomicStateTest, update_by_reference) {
  sut.set(1, 1);
  auto &f = sut.get();
  f.y = 2;
  EXPECT_EQ(sut.get(), Foo(1, 2));
  auto r = sut.load();
  EXPECT_EQ(r, Foo(1, 2));
}

TEST_F(AtomicStateTest, has_changed_works) {
  auto prev = sut.count();
  EXPECT_FALSE(sut.has_changed(prev));
  sut.set(1, 3);
  EXPECT_TRUE(sut.has_changed(prev));
}

TEST_F(AtomicStateTest, reset_retains_content) {
  sut.set(2, 2);
  sut.reset();
  auto v = sut.load();
  EXPECT_EQ(v, Foo(2, 2));
}

TEST_F(AtomicStateTest, set_after_reset) {
  sut.set(2, 2);
  sut.reset();
  sut.set(5, 5);
  auto v = sut.load();
  EXPECT_EQ(v, Foo(5, 5));
}

int NUM_THREADS = 3;

void write(Sut &sut, std::atomic<int> &threads, int iterations) {
  ++threads;
  while (threads != NUM_THREADS)
    ;

  for (int i = 0; i < iterations; ++i) {
    auto val = sut.load();
    val.x++;
    val.y++;
    sut.set(val);
  }
}

void read(Sut &sut, std::atomic<int> &threads, int iterations) {
  ++threads;
  while (threads != NUM_THREADS)
    ;

  for (int i = 0; i < iterations; ++i) {
    auto val = sut.load();
    EXPECT_EQ(val.x, val.y); // the values must be the same; otherwise
                             // the write is non-atomic
  }
}

TEST(AtomicStateConcurrentTest, single_writer_two_readers) {
  int iterations = 1000000;
  std::atomic<int> threads{0};
  Sut sut(0, 0);

  std::thread t1(write, std::ref(sut), std::ref(threads), iterations);
  std::thread t2(read, std::ref(sut), std::ref(threads), iterations);
  std::thread t3(read, std::ref(sut), std::ref(threads), iterations);

  t1.join();
  t2.join();
  t3.join();

  auto val = sut.load();
  EXPECT_EQ(val, Foo(iterations, iterations));
};

} // namespace