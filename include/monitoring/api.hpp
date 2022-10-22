#pragma once

#include "source_location.hpp"
#include "thread_monitor.hpp"
#include "thread_state.hpp"

#include "report.hpp"
#include "stack/allocator.hpp"
#include "stats.hpp"
#include "time.hpp"

#include <assert.h>
#include <chrono>

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
  // TODO: config time
  monitor_instance().start_active_monitoring(interval);
}

void stop_active_monitoring() { monitor_instance().stop_active_monitoring(); }

void start_this_thread_monitoring() {
  // assert(!is_monitored());
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
  data.deadline = to_deadline(timeout);
  tl_state->checkpoint_stack.push(*entry);
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
  data.deadline = to_deadline(timeout);

#ifdef MONITORING_STATS
  // TODO: taking the time twice is bad
  data.start = clock_t::now();
#endif
  tl_state->checkpoint_stack.push(*entry);
}

void confirm_progress(const source_location &location) {
  assert(is_monitored());

  auto now = clock_t::now();
  auto confirm_time = to_time_unit(now);

  auto entry = tl_state->checkpoint_stack.pop();
  assert(entry != nullptr);

  auto &data = entry->data;
  auto deadline = data.deadline.load();

  if (deadline > 0) {
    uint64_t delta;
    if (is_exceeded(deadline, confirm_time, delta)) {
      // deadline violation - should be rare
      self_report_violation(*tl_state, data, delta, location);
    }
    data.deadline.store(0); // to avoid reporting of monitoring thread
  }
#ifdef MONITORING_STATS
  // TODO: check and optimize difference
  auto runtime = now - data.start;
  // TODO: clean duration logic
  auto d = std::chrono::duration_cast<std::chrono::microseconds>(runtime);
  // auto d = std::chrono::duration_cast<time_unit_t>(runtime);
  stats_monitor::update(data.id, d.count());
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
