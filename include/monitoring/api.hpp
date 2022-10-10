#pragma once

#include "source_location.hpp"
#include "thread_monitor.hpp"
#include "thread_state.hpp"

#include "report.hpp"
#include "stack/allocator.hpp"
#include "time.hpp"

#include <assert.h>

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
  // data.id = if provided
  data.deadline = to_deadline(timeout);
  std::cout << "push deadline " << data.deadline << std::endl;
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
  std::cout << "push deadline " << data.deadline << std::endl;
  tl_state->checkpoint_stack.push(*entry);
}

void confirm_progress(const source_location &location) {
  assert(is_monitored());
  auto now = to_time_unit(clock_t::now());

  auto entry = tl_state->checkpoint_stack.pop();
  assert(entry != nullptr);

  auto &data = entry->data;
  auto deadline = data.deadline.load();

  uint64_t delta;
  if (is_exceeded(deadline, now, delta)) {
    // deadline violation - should be rare
    self_report_violation(*tl_state, data, delta, location);
  }

  // no need to call a dtor of a stack_entry
  tl_stack_allocator.deallocate(entry);
}

} // namespace monitor
