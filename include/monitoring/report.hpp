#pragma once

#include "source_location.hpp"
#include "thread_state.hpp"

#include <iostream>

namespace monitor {

void self_report_violation(thread_state &state, checkpoint &check,
                           uint64_t violation_delta,
                           const source_location &location) {
  std::cout << "[This thread] tid " << state.tid << " deadline exceeded by "
            << violation_delta << " time units at CONFIRM PROGRESS in "
            << location;

  if (check.id != 0) {
    std::cout << " checkpoint id " << check.id;
  }
  std::cout << std::endl;
  state.invoke_handler(check);
}

void monitoring_thread_report_violation(thread_state &state, checkpoint &check,
                                        uint64_t violation_delta) {
  std::cout << "[Monitoring thread] deadline exceeded by " << violation_delta;
  // << " time units at EXPECT PROGRESS in " << check.location;

  if (check.id != 0) {
    std::cout << " checkpoint id " << check.id;
  }
  std::cout << std::endl;
  state.invoke_handler(check);
}

} // namespace monitor