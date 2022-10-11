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
class stack {
public:
  stack() : m_top{nullptr} {}

  bool empty() { return m_top == nullptr; }

  void push(stack_entry &entry) {
    entry.count = m_count.fetch_add(1, std::memory_order_acq_rel);

    if (!m_top) {
      entry.next = nullptr;
      m_top = &entry;
      return;
    }

    entry.next = m_top;
    m_top = &entry;
  }

  stack_entry *pop() {
    auto p = m_top.load();
    if (!p) {
      return nullptr;
    }
    m_top.store(p->next);
    return p;
  }

  stack_entry *top() { return m_top.load(); }

  // TODO: synchronization
  uint64_t count() { return m_count.load(); }

  void print() {
    auto p = m_top.load();
    while (p) {
      std::cout << p->count << " ";
      p = p->next;
    }
    std::cout << "eos" << std::endl;
  }

  bool peek(stack_entry &result) {

    // TODO: read only payload data
    stack_entry *t = m_top.load();
    while (t) {
      // TODO: enforce order with fences
      auto oldCount = m_count.load(std::memory_order_acquire);

      // m_top may change but memcpy will succeed (potentially with garbage)
      std::memcpy(&result, &t, sizeof(stack_entry));

      // todo: atomic
      // must happen after memcpy
      if (m_count == oldCount) {
        return true;
      }
      // m_top changed, memcpy may have been invalid, try again
      t = m_top.load();
    }

    return false;
  }

  // TODO: can invoke some function on successfully read entry

  // return true if we iterated over all entries and the stack did not change in
  // the meantime
  bool concurrent_iterate() {

    stack_entry entry;
    stack_entry *t;
    uint64_t oldCount;

    std::cout << "concurrent_iterate ";

    do {
      t = m_top.load();
      if (!t) {
        std::cout << std::endl;
        return true; // no value, i.e. we read them all
      }

      oldCount = m_count.load(std::memory_order_acquire);

      std::memcpy(&entry, t, sizeof(stack_entry));

    } while (m_count != oldCount); // or stop directly at first failure?

    // iterate further while the stack does not change
    do {
      // print successfully read value
      std::cout << entry.count << " ";

      if (entry.next == nullptr) {
        std::cout << std::endl;
        return true; // successfully read all entries
      }
      std::memcpy(&entry, entry.next, sizeof(stack_entry));
    } while (m_count == oldCount);

    // if the m_count changes we consider the memcpy as failed and do not
    // process further entries
    std::cout << "\nconcurrent_iterate incomplete - entries changed"
              << std::endl;

    return false;
  }

private:
  // we want to read the stack from another thread (peek)
  // atomic needed if we sync with m_count?
  std::atomic<stack_entry *> m_top{nullptr};
  std::atomic<uint64_t> m_count{0};
};

} // namespace monitor
