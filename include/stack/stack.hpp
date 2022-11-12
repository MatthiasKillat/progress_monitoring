#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <type_traits>

#include "entry.hpp"

namespace monitor {

// stack structure is only modified in one context (push/pop)
// but read in others (could be many readers)
// more precisely the other contexts may also modify data on the stack, but not
// call push/pop
class deadline_stack {
public:
  deadline_stack() : m_top{nullptr} {}

  bool empty() { return top() == nullptr; }

  void push(stack_entry &entry) {
    entry.count = m_count.fetch_add(1, std::memory_order_relaxed);

    // ensure that count is increased before stack is changed
    std::atomic_thread_fence(std::memory_order_release);

    // we only change top in one thread and hence do not have to snychronize
    auto p = m_top.load(std::memory_order_relaxed);

    // if (!p) {
    //   entry.next = nullptr;
    //   m_top.store(&entry, std::memory_order_release);
    //   return;
    // }

    entry.next = p;
    m_top.store(&entry, std::memory_order_release);
  }

  stack_entry *pop() {
    auto p = m_top.load(std::memory_order_relaxed);
    if (!p) {
      return nullptr;
    }
    m_top.store(p->next, std::memory_order_release);
    return p;
  }

  stack_entry *top() { return m_top.load(std::memory_order_acquire); }

  uint64_t count() { return m_count.load(std::memory_order_relaxed); }

  bool peek(stack_entry &result) {

    stack_entry *p = m_top.load();
    while (p) {
      auto oldCount = count();

      // ensure that count is read before the data is copied
      std::atomic_thread_fence(std::memory_order_acquire);

      // m_top may change but memcpy will succeed (potentially with garbage)
      std::memcpy(&result, &p, sizeof(stack_entry));

      // potentially redundant but issues no instruction on x86
      std::atomic_thread_fence(std::memory_order_release);

      if (count() == oldCount) {
        return true;
      }
      // m_top changed, memcpy may have been invalid, try again
      p = m_top.load();
    }

    return false;
  }

private:
  // we want to read the stack from another thread (peek)
  // atomic needed if we sync with m_count?
  std::atomic<stack_entry *> m_top{nullptr};
  std::atomic<uint64_t> m_count{0};
};

} // namespace monitor
