#pragma once

#include <functional>
#include <mutex>
#include <thread>

#include "stack/stack.hpp"

namespace monitor {

using thread_id_t = std::thread::id;
using index_t = uint32_t;

class thread_monitor;

// weak/no encapsulation for simplicity and performance
struct thread_state {

  // nested functions require a lock-free stack,
  // suitable for one writer and one concurrent reader
  deadline_stack deadlines;
  thread_id_t tid{0};

  index_t index;

  thread_monitor *monitor{nullptr};

  thread_state() = default;
  thread_state(const thread_state &other) = delete;

  void lock() { m_mutex.lock(); }
  void unlock() { m_mutex.unlock(); }

  template <typename Handler> void set_handler(const Handler &handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = handler;
  }

  void unset_handler() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = 0;
  }

  void invoke_handler(checkpoint &check) {
    // can happen from monitoring thread, but only in the case of
    // a deadline violation (rare)
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_handler)
      m_handler(check);
  }

private:
  std::function<void(checkpoint &)> m_handler;

  // the hot monitoring path is lock-free, but there are low-contention locks
  // replacement with lock-free construct would perform worse
  // and be more complicated
  std::mutex m_mutex;
};

} // namespace monitor
