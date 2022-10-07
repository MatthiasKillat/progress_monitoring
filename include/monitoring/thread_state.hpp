#pragma once

#include <functional>
#include <mutex>
#include <thread>

#include "stack/stack.hpp"

namespace monitor {

using thread_id_t = std::thread::id;
using index_t = uint32_t;

// weak/no encapsulation for simplicity and performance
struct thread_state {
  // nested functions require a lock-free checkpoint stack,
  // suitable for one writer and one concurrent reader
  stack checkpoint_stack;
  thread_id_t tid{0};

  index_t index;
  std::atomic<uint64_t> aba{0}; // needed?

  thread_state() = default;
  thread_state(const thread_state &other) = delete;

  void lock() { m_mutex.lock(); }
  void unlock() { m_mutex.unlock(); }

  template <typename Handler> void set_handler(const Handler &handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = handler;
  }

  void invoke_handler(checkpoint &check) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_handler)
      m_handler(check);
  }

private:
  std::function<void(checkpoint &)> m_handler;

  // the hot monitoring path is lock-free, but there are low-contention locks
  // these are low contended
  // replacement with lock-free construct would perform worse
  // and be more complicated
  std::mutex m_mutex;
};

} // namespace monitor
