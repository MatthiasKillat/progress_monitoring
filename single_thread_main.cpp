#include <chrono>
#include <iostream>

// OFF     : disables monitoring (no overhead in application code)
//
// PASSIVE : only the current thread itself can detect a violation
//           - low overhead
//
// ACTIVE  : the current thread or a dedicated monitoring thread
// can detect a violation (first one detects it)
// - more overhead but faster response and ability to react on potential
// deadlocks

// #define MONITORING_OFF
#define MONITORING_ACTIVE
// #define MONITORING_PASSIVE

#include "monitoring_interface.hpp"

using namespace std::chrono_literals;

constexpr auto TIME_BUDGET = 100ms;

// some of them will provoke a deadline violation
constexpr auto ARTIFICIAL_DELAY = TIME_BUDGET - 1ms;
// constexpr auto ARTIFICIAL_DELAY = TIME_BUDGET;
// constexpr auto ARTIFICIAL_DELAY = TIME_BUDGET + 1ms;
// constexpr auto ARTIFICIAL_DELAY = TIME_BUDGET + 50ms;

int add(int a, int b) {
  std::this_thread::sleep_for(ARTIFICIAL_DELAY);
  return a + b;
}

int main(int argc, char **argv) {

  // initialize the monitoring with a certain rate
  ACTIVATE_MONITORING(100ms); // low frequency to save cycles

  // start to monitor current thread
  START_MONITORING;

  // start of a critical section that needs to complete
  // in a certain time
  EXPECT_PROGRESS_IN(TIME_BUDGET);

  auto result = add(2, 3);

  // end of the critical section
  CONFIRM_PROGRESS;

  std::cout << "result = " << result << std::endl;

  // stop monitoring current thread
  STOP_MONITORING;

  // deactivate monitoring mechanism
  DEACTIVATE_MONITORING;
  return 0;
}