#include <gtest/gtest.h>

#include "monitoring/macros.hpp"

#include <chrono>

using namespace std::chrono_literals;

std::atomic<bool> g_deadline_violation{false};

void handler(monitor::checkpoint &) {
  // we could e.g. extract the id of the violation from the checkpoint
  std::cout << "HANDLER CALLED" << std::endl;
  g_deadline_violation = true;
}

#define CHECK_PROGRESS                                                         \
  do {                                                                         \
    CONFIRM_PROGRESS;                                                          \
    EXPECT_FALSE(g_deadline_violation);                                        \
  } while (0)

class MonitoringTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    g_deadline_violation = false;
    // we only monitor the main test thread in this example

    // TODO: part of this is better done only once in main
    START_ACTIVE_MONITORING(100ms);
    START_THIS_THREAD_MONITORING;
    SET_MONITORING_HANDLER(handler);
  }

  virtual void TearDown() {
    STOP_THIS_THREAD_MONITORING;
    STOP_ACTIVE_MONITORING;
  }
};

TEST_F(MonitoringTest, probably_in_time) {

  // the test code is supposed to have run to completion
  // in 100ms, otherwise the test fails
  EXPECT_PROGRESS_IN(100ms, 1);

  std::this_thread::sleep_for(99ms);

  CHECK_PROGRESS;
}

TEST_F(MonitoringTest, deadline_violation) {

  // the test code is supposed to have run to completion
  // in 100ms, otherwise the test fails
  EXPECT_PROGRESS_IN(100ms, 1);

  std::this_thread::sleep_for(101ms);

  CHECK_PROGRESS;
}
