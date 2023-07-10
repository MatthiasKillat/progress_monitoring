#include <gtest/gtest.h>

#include "state/single_wait.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace {

using Waitable = SingleWait;
using signal_t = Waitable::signal_t;

class WaitNotifyTest : public ::testing::Test {
protected:
  virtual void SetUp() {}

  virtual void TearDown() {}

  WaitState state;
  Waitable waitable{state};
  Notifier notifier{state};
};

void wait(Waitable &waitable, signal_t expected) {
  auto signal = waitable.wait();
  EXPECT_EQ(signal, expected);
}

void wait_twice(Waitable &waitable, signal_t expected) {
  auto signal = waitable.wait();
  signal = waitable.wait();

  EXPECT_EQ(signal, expected);
}

TEST_F(WaitNotifyTest, notify) {
  signal_t signal = 73;
  std::thread t(wait, std::ref(waitable), signal);

  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  EXPECT_EQ(waitable.count(), 0);
  // may happen before or after wait (even with sleep)
  notifier.notify(signal);

  EXPECT_EQ(waitable.count(), 1);

  t.join();
}

TEST_F(WaitNotifyTest, notify_before_wait) {
  signal_t signal = 73;

  EXPECT_EQ(waitable.count(), 0);
  notifier.notify(signal);

  std::thread t(wait, std::ref(waitable), signal);

  t.join();
  EXPECT_EQ(waitable.count(), 1);
}

TEST_F(WaitNotifyTest, notify_after_wait) {
  signal_t signal = 73;

  EXPECT_EQ(waitable.count(), 0);

  std::thread t(wait_twice, std::ref(waitable), signal);
  notifier.notify(signal + 1); // some other bit is also set

  while (!notifier.is_waiting())
    ;

  // reset by first wake-up, is waiting again

  notifier.notify(signal);
  t.join();

  EXPECT_EQ(waitable.count(), 2);
}

TEST_F(WaitNotifyTest, aggregate_signals) {
  signal_t signal1 = 4;
  signal_t signal2 = 9;
  signal_t signal = signal1 | signal2;

  EXPECT_EQ(waitable.count(), 0);

  notifier.notify(signal1);
  notifier.notify(signal2);
  std::thread t(wait, std::ref(waitable), signal);

  t.join();

  EXPECT_EQ(waitable.count(), 2);
}

} // namespace