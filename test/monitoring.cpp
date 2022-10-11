#include <gtest/gtest.h>

#include "monitoring/macros.hpp"

#include <chrono>

using namespace std::chrono_literals;

bool g_deadline_violation{false};

void handler(monitor::checkpoint &) {
  // we could e.g. extract the id of the violation from the checkpoint
  g_deadline_violation = true;
}

class MonitoringTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    g_deadline_violation = false;
    // we only monitor the main test thread in this example

    // TODO: part of this is better done only once in main
    START_ACTIVE_MONITORING(100ms);
    START_THREAD_MONITORING;
    SET_MONITORING_HANDLER(handler);
  }

  virtual void TearDown() {
    EXPECT_FALSE(g_deadline_violation);
    STOP_THREAD_MONITORING;
    STOP_ACTIVE_MONITORING;
  }
};

// this covers timeout, i.e. test code that eventually returns
// for a potential deadlock it gets harder, as we would need
// to terminate a thread that is potentially in deadlock
TEST_F(MonitoringTest, deadline_violation) {

  // the test code is supposed to have run to completion
  // in 100ms, otherwise the test fails
  EXPECT_PROGRESS_IN(100ms, 1);

  std::this_thread::sleep_for(101ms);

  // note that it matches to any checkpoint id before
  // (relating it to a specific id raises design issues)
  CONFIRM_PROGRESS;
}