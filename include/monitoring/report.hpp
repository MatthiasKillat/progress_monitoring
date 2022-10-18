#pragma once

#include "source_location.hpp"
#include "thread_state.hpp"

#include "config.hpp"

#include <iostream>

namespace monitor {

void self_report_violation(thread_state &state, checkpoint &check,
                           uint64_t violation_delta,
                           const source_location &location) {
#ifdef DEADLINE_VIOLATION_OUTPUT_ON
  std::cout << "[This thread] tid " << state.tid << " deadline exceeded by "
            << violation_delta << " time units at CONFIRM PROGRESS in "
            << location;

  if (check.id != 0) {
    std::cout << " checkpoint id " << check.id;
  }
  std::cout << std::endl;
#else
  (void)violation_delta;
  (void)location;
#endif
  state.invoke_handler(check);
}

void monitoring_thread_report_violation(thread_state &state, checkpoint &check,
                                        uint64_t violation_delta) {
#ifdef DEADLINE_VIOLATION_OUTPUT_ON
  std::cout << "[Monitoring thread] deadline exceeded by at least "
            << violation_delta << " time units at " << check.location;

  if (check.id != 0) {
    std::cout << " checkpoint id " << check.id;
  }
  std::cout << std::endl;
#else
  (void)violation_delta;
  (void)check;
#endif
  state.invoke_handler(check);
}

} // namespace monitor