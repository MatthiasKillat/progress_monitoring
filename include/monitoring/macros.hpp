#pragma once

#include "api.hpp"

#define MONITORING_MODE_PASSIVE
#define MONITORING_MODE_ACTIVE

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

#ifdef MONITORING_MODE_ACTIVE

// we always need to passively monitor to ensure we detect each deadline
// violation
#define MONITORING_PASSIVE
#endif

#ifdef MONITORING_PASSIVE

// no function syntax if there are no arguments

#define START_MONITORING                                                       \
  do {                                                                         \
    monitor::start_this_thread_monitoring();                                   \
  } while (0)

#define STOP_MONITORING                                                        \
  do {                                                                         \
    monitor::stop_this_thread_monitoring();                                    \
  } while (0)

#define SET_MONITORING_HANDLER(handler)                                        \
  do {                                                                         \
    monitor::set_this_thread_handler(handler);                                 \
  } while (0)

#define UNSET_MONITORING_HANDLER                                               \
  do {                                                                         \
    monitor::unset_this_thread_handler();                                      \
  } while (0)

#define EXPECT_PROGRESS_IN(timeout, checkpoint_id)                             \
  do {                                                                         \
    monitor::expect_progress_in(timeout, checkpoint_id, THIS_SOURCE_LOCATION); \
  } while (0)

#define CONFIRM_PROGRESS                                                       \
  do {                                                                         \
    monitor::confirm_progress(THIS_SOURCE_LOCATION);                           \
  } while (0)

#endif

#ifdef MONITORING_MODE_ACTIVE

#define START_ACTIVE_MONITORING(interval)                                      \
  { monitor::start_active_monitoring(interval); }                              \
  while (0)

#define STOP_ACTIVE_MONITORING                                                 \
  { monitor::stop_active_monitoring(); }                                       \
  while (0)

#else

#define START_ACTIVE_MONITORING(interval)
#define STOP_ACTIVE_MONITORING

#endif

#endif