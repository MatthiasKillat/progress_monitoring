#include <gtest/gtest.h>

#include "monitoring/macros.hpp"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

std::atomic<int> g_deadline_violations{0};

void handler(monitor::checkpoint &) {
  // we could e.g. extract the id of the violation from the checkpoint
  ++g_deadline_violations;
}

#define EXPECT_DEADLINE_MET                                                    \
  do {                                                                         \
    CONFIRM_PROGRESS;                                                          \
    EXPECT_EQ(g_deadline_violations, 0);                                       \
  } while (0)

#define EXPECT_DEADLINE_VIOLATION                                              \
  do {                                                                         \
    CONFIRM_PROGRESS;                                                          \
    EXPECT_GE(g_deadline_violations, 0);                                       \
  } while (0)

class MonitoringTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    g_deadline_violations = 0;
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

  // may fail due to scheduling/sleep precision
  EXPECT_DEADLINE_MET;
}

TEST_F(MonitoringTest, deadline_violation) {

  // the test code is supposed to have run to completion
  // in 100ms, otherwise the test fails
  EXPECT_PROGRESS_IN(100ms, 1);

  std::this_thread::sleep_for(101ms);

  // should always succeed, i.e. violate the deadline
  EXPECT_DEADLINE_VIOLATION;
}

std::atomic<bool> g_run;

void work() {
  START_THIS_THREAD_MONITORING;
  SET_MONITORING_HANDLER(handler);

  EXPECT_PROGRESS_IN(100ms, 1);

  while (g_run)
    ;

  CONFIRM_PROGRESS;
  STOP_THIS_THREAD_MONITORING;
}

TEST_F(MonitoringTest, deadlock_leads_to_violation) {

  g_run = true;

  // will not return without being unlocked
  std::thread t(work);

  std::this_thread::sleep_for(500ms);

  g_run = false;
  t.join();

  EXPECT_EQ(g_deadline_violations, 1);
}
