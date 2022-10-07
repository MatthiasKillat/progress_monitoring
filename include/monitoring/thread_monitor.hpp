#pragma once

#include "thread_state.hpp"

#include <stdint.h>

#include <algorithm>
#include <array>
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
    m_registered.push_back(index);

    return &state;
  }

  void deregister(thread_state &state) {
    // we expect it to be registered (misuse otherwise)
    std::lock_guard<thread_monitor> g(*this);
    auto index = state.index;
    auto iter = std::find(m_registered.begin(), m_registered.end(), index);
    m_registered.erase(iter);
    deinit(state);
    m_free.push(index);
  }

  void start_active_monitoring() {}

  void stop_active_monitoring() {}

  void lock() { m_mutex.lock(); }
  void unlock() { m_mutex.unlock(); }

private:
  // weakly contended, only for registration and deregistration
  // resgitration without mutex but only atomics is complicated
  // and not worth it
  std::mutex m_mutex;

  std::array<thread_state, Capacity> m_states;
  std::queue<index_t> m_free;

  // cannot efficiently be searched but traversal is more important
  std::list<index_t> m_registered;

  thread_state &get_state(index_t index) { return m_states[index]; }

  void init(thread_state &state) {
    // state.aba = 0;
    state.tid = std::this_thread::get_id();
  }

  void deinit(thread_state &state) {
    state.tid = thread_id_t();
    state.checkpoint_stack.clear();
  }
};

} // namespace monitor