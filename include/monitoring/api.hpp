#pragma once

#include "source_location.hpp"
#include "thread_monitor.hpp"
#include "thread_state.hpp"

#include "report.hpp"
#include "stack/allocator.hpp"
#include "statistics.hpp"
#include "time.hpp"

#include <assert.h>
#include <chrono>
#include <ctime>

namespace monitor {

using monitor = thread_monitor;

// TODO: better singleton approach
thread_local thread_state *tl_state{nullptr};
thread_local stack_allocator tl_stack_allocator;

monitor &monitor_instance() {
  static monitor instance;
  return instance;
}

bool is_monitored() { return tl_state != nullptr; }

void start_active_monitoring(time_unit_t interval) {
  monitor_instance().start_active_monitoring(interval);
}

void stop_active_monitoring() { monitor_instance().stop_active_monitoring(); }

void start_this_thread_monitoring() {
  assert(!is_monitored());
  tl_state = monitor_instance().register_this_thread();

  if (!tl_state) {
    std::cerr << "THREAD MONITORING ERROR - maximum threads exceeded"
              << std::endl;
    std::terminate();
  }
}

void stop_this_thread_monitoring() {
  assert(is_monitored());
  monitor_instance().deregister(*tl_state);
  tl_state = nullptr;
}

template <typename H> void set_this_thread_handler(H &handler) {
  assert(is_monitored());
  tl_state->set_handler(handler);
}

void unset_this_thread_handler() {
  assert(is_monitored());
  tl_state->unset_handler();
}

void expect_progress_in(time_unit_t timeout, const source_location &location) {
  assert(is_monitored());

  auto *entry = tl_stack_allocator.allocate();

  // TODO: use monitoring fatal assert
  if (!entry) {
    std::cerr << "MONITORING ERROR - stack allocation error" << std::endl;
    std::terminate();
  }

  // placement new
  new (entry) stack_entry;

  auto &data = entry->data;
  data.location = location;
  data.id = 0;
  auto d = to_deadline(timeout);
  data.deadline = d;
  data.deadline_validator = d;
  tl_state->deadlines.push(*entry);

  // needed if we use some kind of adaptive deadline scheme
  // this is too costly to be worth it
  // monitor_instance().wake_up();
}

void expect_progress_in(time_unit_t timeout, checkpoint_id_t check_id,
                        const source_location &location) {
  assert(is_monitored());

  auto *entry = tl_stack_allocator.allocate();

  // TODO: use monitoring fatal assert
  if (!entry) {
    std::cerr << "MONITORING ERROR - stack allocation error" << std::endl;
    std::terminate();
  }
  // placement new
  new (entry) stack_entry;

  auto &data = entry->data;
  data.location = location;
  data.id = check_id;
  auto d = to_deadline(timeout);
  data.deadline = d;
  data.deadline_validator = d;

#ifdef MONITORING_STATS
  // TODO: taking the time twice is bad
  data.start = clock_t::now();
  // data.start = unow();
#endif
  tl_state->deadlines.push(*entry);

  // this is too costly to be worth it
  // monitor_instance().wake_up();
}

void confirm_progress(const source_location &location) {
  assert(is_monitored());
  auto now = clock_t::now();
  // auto now = unow();
  auto confirm_time = to_time_unit(now);

  auto entry = tl_state->deadlines.pop();
  assert(entry != nullptr);

  auto &data = entry->data;
  auto deadline = data.deadline.load();

// TODO: ifdefs are bad, refactor (we still want efficiency...)
#ifdef MONITORING_STATS
  bool exceeded{false};
#endif

  if (deadline == data.deadline_validator) {
    uint64_t delta;
    if (is_violated(deadline, confirm_time, delta)) {
      // deadline violation - should be rare
#ifdef MONITORING_STATS
      exceeded = true;
#endif
      self_report_violation(*tl_state, data, delta, location);
      // TODO: conditional
      monitor_instance().invoke_handler(data);
    }
    // to avoid reporting of monitoring thread, note that the monitoring thread
    // increments the other variable
    data.deadline.fetch_sub(1);
  }
#ifdef MONITORING_STATS
  else {
    exceeded = true;
  }
  // TODO: check and optimize difference
  auto runtime = now - data.start;
  // TODO: clean duration logic
  auto d = std::chrono::duration_cast<std::chrono::microseconds>(runtime);
  // auto d = std::chrono::duration_cast<time_unit_t>(runtime);
  stats_monitor::update(data.id, d.count(), exceeded);
#endif

  // no need to call a dtor of a stack_entry
  tl_stack_allocator.deallocate(entry);
}

// TODO: implement and test
class guard {

public:
  guard(time_unit_t timeout, checkpoint_id_t check_id,
        const source_location &location) {

    m_location = location;
    expect_progress_in(timeout, check_id, m_location);
  }

  guard(guard &) = delete;

  ~guard() { confirm_progress(m_location); }

private:
  source_location m_location;
};

void print_stats() {
#ifdef MONITORING_STATS
  stats_monitor::print();
#endif
}

} // namespace monitor
