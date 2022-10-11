#pragma once

#include "report.hpp"
#include "thread_state.hpp"
#include "time.hpp"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <list>
#include <mutex>
#include <queue>
#include <thread>

namespace monitor {

// TODO: config
constexpr uint32_t MAX_THREADS = 2;

class thread_monitor {
  static constexpr uint32_t Capacity = MAX_THREADS;

public:
  thread_monitor() {
    for (index_t i = 0; i < Capacity; ++i) {
      m_free.push(i);
      auto &state = get_state(i);
      state.index = i;
    }
  }

  ~thread_monitor() {}

  thread_state *register_this_thread() {
    std::lock_guard<thread_monitor> g(*this);

    if (m_free.empty()) {
      return nullptr;
    }

    auto index = m_free.front();
    m_free.pop();

    auto &state = get_state(index);
    init(state);
    m_registered.push_back(&state);
    return &state;
  }

  void deregister(thread_state &state) {
    // we expect it to be registered (misuse otherwise)
    std::lock_guard<thread_monitor> g(*this);
    auto index = state.index;
    auto iter = std::find(m_registered.begin(), m_registered.end(), &state);
    m_registered.erase(iter);
    deinit(state);
    m_free.push(index);
  }

  void start_active_monitoring(time_unit_t interval) {
    if (!m_active) {
      m_interval = interval;
      m_active = true;

      m_thread = std::thread(&thread_monitor::monitor_loop, this);
      prioritize(m_thread);
    }
  }

  void stop_active_monitoring() {
    if (m_active) {
      m_active = false;
      m_thread.join();
    }
  }

  void lock() { m_mutex.lock(); }
  void unlock() { m_mutex.unlock(); }

private:
  // weakly contended, only for registration and deregistration
  // resgitration without mutex but only atomics is complicated
  // and not worth it
  std::mutex m_mutex;

  std::array<thread_state, Capacity> m_states;
  std::queue<index_t> m_free;

  // TODO: bad for locality and insertion, optimize later
  std::list<thread_state *> m_registered;

  std::atomic_bool m_active{false};
  std::thread m_thread;
  time_unit_t m_interval{10};

  thread_state &get_state(index_t index) { return m_states[index]; }

  void init(thread_state &state) { state.tid = std::this_thread::get_id(); }

  void deinit(thread_state &state) {
    state.tid = thread_id_t();
    auto &s = state.checkpoint_stack;
    stack_entry *entry = s.pop();
    while (entry) {
      // TODO: deallocate, need stack allocator for that
      entry = s.pop();
    }
  }

  void prioritize(std::thread &thread) {
    // works only on linux for now
    auto h = thread.native_handle();
    constexpr int policy = SCHED_FIFO;
    sched_param params;
    params.sched_priority = sched_get_priority_max(policy);
    auto result = pthread_setschedparam(h, policy, &params);
    if (result != 0) {
      // requires e.g. root rights
      std::cerr
          << "MONITORING ERROR - setting monitoring thread priority failed"
          << std::endl;
    }
  }

  void check_deadlines(const std::chrono::time_point<clock_t> &checktime) {
    auto time = to_time_unit(checktime);

    // this lock is weakly contended (only at registration/deregistration)
    // TODO: do we need to optimize here?
    std::lock_guard<thread_monitor> g(*this);

    // TODO: optimize iteration structure
    for (auto state : m_registered) {
      auto &stack = state->checkpoint_stack;

      // TODO: analyze whether stronger fences are needed!
      auto old_count = stack.count();

      auto entry = stack.top();
      // we check the stack entries for violations
      // TODO: skip unnecessary checks (known violations), but this requires
      // a more complex way of storing the violations (worth it?)...
      while (entry && check_entry(*state, *entry, old_count, time)) {
        entry = entry->next;
      }
    }
  }

  void monitor_loop() {
    while (m_active) {
      auto now = clock_t::now();
      auto wakeup = now + m_interval;
      check_deadlines(now);
      std::this_thread::sleep_until(wakeup);
    }
  }

  // factored out, returns whether to continue checking
  bool check_entry(thread_state &state, stack_entry &entry, uint64_t old_count,
                   time_t time) {
    auto &stack = state.checkpoint_stack;
    auto deadline = entry.data.deadline.load(std::memory_order_relaxed);

    if (old_count != stack.count()) {
      return false; // stack changed (push or pop of a deadline), skip check for
                    // this iteration
    }

    if (deadline == 0) {
      return true; // check next deadline if any
    }

    uint64_t delta;
    if (is_exceeded(deadline, time, delta)) {
      // reset original, the memory exists (TODO: there is a ABA problem if
      // the entry is recycled, TODO: are the consequences harmful? (false
      // positove/negative/corruption?))

      if (entry.data.deadline.compare_exchange_strong(
              deadline, 0, std::memory_order_acq_rel,
              std::memory_order_relaxed)) {
        monitoring_thread_report_violation(state, entry.data, delta);
        return true;
      }
    }

    return false;
  }
};

} // namespace monitor