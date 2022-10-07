#pragma once

#include "thread_monitor.hpp"
#include "thread_state.hpp"

namespace monitor {

using monitor = thread_monitor;

// TODO: better singleton approach
thread_local thread_state *tl_state{nullptr};

monitor &monitor_instance() {
  static monitor instance;
  return instance;
}

void start_monitoring() {
  tl_state = monitor_instance().register_this_thread();

  if (!tl_state) {
    std::cerr << "THREAD MONITORING ERROR - maximum threads exceeded"
              << std::endl;
    std::terminate();
  }
}

void stop_monitoring() {
  monitor_instance().deregister(*tl_state);
  tl_state = nullptr;
}

bool is_monitored() { return tl_state != nullptr; }

// /// @brief stops the monitoring of the current (i.e. calling) thread
// void stopMonitoring();

// /// @brief indicates whether the current thread is monitored
// bool isMonitored();

// /// @brief indicates that the current thread awaits to progress to the
// /// next conformation checkpoint in timeout units of time
// void awaitProgressIn(time_unit_t timeout);

// /// @brief indicates that the current thread awaits to progress to the
// /// next conformation checkpoint in timeout units of time,
// /// provides the source location for additional output in the timeout case
// void awaitProgressIn(time_unit_t timeout, const SourceLocation &location);

// /// @brief confirms that the current thread has progressed to the next
// /// checkpoint, outputs timeout message if there is a timeout detected by the
// /// current thread
// void confirmProgress(const SourceLocation &location);

} // namespace monitor
