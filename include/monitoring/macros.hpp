#pragma once

#include "thread_monitor.hpp"

#pragma once

#include "monitoring.hpp"

#define MONITORING_ON
#define MONITORING_PASSIVE

#ifdef MONITORING_OFF

#define START_MONITORING
#define STOP_MONITORING
#define SET_HANDLER
#define EXPECT_PROGRESS_IN(deadline)
#define CONFIRM_PROGRESS
#define SET_DEADLINE_HANDLER(handler)

#define ACTIVATE_MONITORING(interval)
#define DEACTIVATE_MONITORING

#else

#ifdef MONITORING_ACTIVE

// we always need to passively monitor to ensure we detect each deadline
// violation
#define MONITORING_PASSIVE
#endif

#ifdef MONITORING_PASSIVE

#define START_MONITORING                                                       \
  { monitor::startMonitoring(); }

#define STOP_MONITORING                                                        \
  { monitor::stopMonitoring(); }

#define SET_DEADLINE_HANDLER(handler)                                          \
  { monitor::setHandler(handler); }

#define EXPECT_PROGRESS_IN(deadline)                                           \
  { monitor::awaitProgressIn(deadline, CURRENT_SOURCE_LOCATION); }

#define CONFIRM_PROGRESS                                                       \
  { monitor::confirmProgress(CURRENT_SOURCE_LOCATION); }

#endif

#ifdef MONITORING_ACTIVE

#define ACTIVATE_MONITORING(interval)                                          \
  { monitor::threadMonitor().start(interval); }

#define DEACTIVATE_MONITORING                                                  \
  { monitor::threadMonitor().stop(); }

#else

#define ACTIVATE_MONITORING(interval)
#define DEACTIVATE_MONITORING

#endif

#endif