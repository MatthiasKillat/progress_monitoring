#pragma once

#include "report.hpp"
#include "thread_state.hpp"
#include "time.hpp"

#include <limits>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <queue>
#include <thread>

namespace monitor {

// TODO: config
constexpr uint32_t MAX_THREADS = 128;

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
      m_max_interval = interval;
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

  void wake_up() {
    // we do not use the mutex as we do not care about lost wake ups
    // TODO: ok? we could also only wake up like this if
    // the regular wake up would be late
    m_wakeup.store(true, std::memory_order_relaxed);
    m_condvar.notify_one();
  }

  // TODO: maybe wake up

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
  time_unit_t m_max_interval{1};
  time_unit_t m_interval{1};

  std::mutex m_thread_mutex;
  std::condition_variable m_condvar;
  std::atomic_bool m_wakeup{false};

  thread_state &get_state(index_t index) { return m_states[index]; }

  void init(thread_state &state) { state.tid = std::this_thread::get_id(); }

  void deinit(thread_state &state) {
    state.tid = thread_id_t();
    // TODO: stack winks out, ok since thread local allocator will also go in
    // normal use case otherwise we must return the entries to the allocator
    state.deadlines.clear();
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

  time_t check_deadlines(const std::chrono::time_point<clock_t> &checktime) {
    auto time = to_time_unit(checktime);

    // this lock is weakly contended (only at registration/deregistration)
    // TODO: do we need to optimize here?
    std::lock_guard<thread_monitor> g(*this);

    auto min_deadline = std::numeric_limits<time_t>::max();

    // TODO: optimize iteration structure
    for (auto state : m_registered) {
      auto &stack = state->deadlines;

      // TODO: analyze whether stronger fences are needed!
      auto old_count = stack.count();

      auto entry = stack.top();

      // we check the stack entries for violations
      // TODO: skip unnecessary checks (known violations), but this requires
      // a more complex way of storing the violations (worth it?)...
      time_t deadline = INVALID_TIME;
      while (entry) {
        bool continue_checking =
            check_entry(*state, *entry, old_count, time, deadline);

        if (continue_checking) {
          entry = entry->next;
        } else {
          // TOdO: used for adaptive wait, probably not very useful and too
          // complex
          if (deadline != INVALID_TIME && deadline < min_deadline) {
            min_deadline = deadline;
          }
          break;
        }
      }
    }

    return min_deadline;
  }

  void sleep() {}

  void monitor_loop() {
    while (m_active) {
      auto now = clock_t::now();
      auto min = check_deadlines(now);

      auto d = to_deadline(m_interval);

      auto wakeup_time = now + m_interval;
      // std::unique_lock<std::mutex> lock(m_thread_mutex);
      // m_wakeup.store(false, std::memory_order_relaxed);
      // m_condvar.wait_until(lock, wakeup_time, [&]() {
      //   return this->m_wakeup.load(std::memory_order_relaxed);
      // });
      std::this_thread::sleep_until(wakeup_time);
    }
  }

  // factored out, returns whether to continue checking
  bool check_entry(thread_state &state, stack_entry &entry, uint64_t old_count,
                   time_t time, time_t &deadline) {
    auto &stack = state.deadlines;
    deadline = entry.data.deadline.load(std::memory_order_relaxed);

    if (old_count != stack.count()) {
      return false; // stack changed (push or pop of a deadline), skip check
                    // for this iteration
    }

    if (deadline == INVALID_TIME) {
      return true; // was already checked by thread itself
    }

    uint64_t delta;
    if (is_exceeded(deadline, time, delta)) {
      // reset original, the memory exists (TODO: there is a ABA problem if
      // the entry is recycled, TODO: are the consequences harmful? (false
      // positive/negative/corruption?))

      if (entry.data.deadline.compare_exchange_strong(
              deadline, INVALID_TIME, std::memory_order_acq_rel,
              std::memory_order_relaxed)) {
        monitoring_thread_report_violation(state, entry.data, delta);
        return true;
      }
    }

    return false;
  }
};

} // namespace monitor