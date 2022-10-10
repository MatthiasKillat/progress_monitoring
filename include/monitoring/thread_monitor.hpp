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

    return &state;
  }

  void deregister(thread_state &state) {
    // we expect it to be registered (misuse otherwise)
    std::lock_guard<thread_monitor> g(*this);
    auto index = state.index;
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

  std::atomic_bool m_active{false};
  std::thread m_thread;
  time_unit_t m_interval{10};

  thread_state &get_state(index_t index) { return m_states[index]; }

  void init(thread_state &state) {
    state.tid = std::this_thread::get_id();
    state.used = true; // can we use the null tid instead?
  }

  void deinit(thread_state &state) {
    state.tid = thread_id_t();
    state.checkpoint_stack.clear();
    state.used = false;
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
    // TOD: do we need to optimize here?
    std::lock_guard<thread_monitor> g(*this);

    stack_entry entry;
    uint64_t delta;

    // TODO: optimize iteration structure
    for (auto &state : m_states) {
      if (state.used) {
        // TODO: optimize, we may not need to copy, ust check the deadline and
        // counter

        // TODO: Broken, repair
        if (state.checkpoint_stack.peek(entry)) {
          auto deadline = entry.data.deadline.load(std::memory_order_relaxed);

          if (deadline > 0 && is_exceeded(deadline, time, delta)) {
            // reset original, the memory exists
            if (entry.data.deadline.compare_exchange_strong(
                    deadline, 0, std::memory_order_acq_rel,
                    std::memory_order_relaxed)) {
              monitoring_thread_report_violation(state, entry.data, delta);
            }

            // TODO: check remainder of stack
          }
        }

        // assume monotonic deadlines on stack, avoid checking whole stack
        // unless there is a violation
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
};

} // namespace monitor