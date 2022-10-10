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

// only modified in one context
class stack {
public:
  stack() : top{nullptr} {}

  void push(stack_entry &entry) {
    entry.count = count; // must happen before stack actually changes (can
                         // lead to false positives which is ok as it only
                         // invalidates peeks)

    count.fetch_add(1, std::memory_order_acq_rel);
    // a peek can fail even if the top was not yet changed
    // if it read the old value before the increment

    if (!top) {
      entry.next = nullptr;
      top = &entry;
      return;
    }

    entry.next = top;
    top = &entry;
  }

  stack_entry *pop() {
    auto p = top.load();
    if (!p) {
      return nullptr;
    }
    top.store(p->next);
    return p;
  }

  void print() {
    auto p = top.load();
    while (p) {
      std::cout << p->count << " ";
      p = p->next;
    }
    std::cout << "eos" << std::endl;
  }

  // using optional would require an additional copy
  bool peek(stack_entry &result) {

    // TODO: read only payload data
    stack_entry *t = top.load();
    while (t) {
      // TODO: enforce order with fences
      auto oldCount = count.load(std::memory_order_acquire);

      // top may change but memcpy will succeed (potentially with garbage)
      std::memcpy(&result, &t, sizeof(stack_entry));

      // todo: atomic
      // must happen after memcpy
      if (count == oldCount) {
        return true;
      }
      // top changed, memcpy may have been invalid, try again
      t = top.load();
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
      t = top.load();
      if (!t) {
        std::cout << std::endl;
        return true; // no value, i.e. we read them all
      }

      oldCount = count.load(std::memory_order_acquire);

      std::memcpy(&entry, t, sizeof(stack_entry));

    } while (count != oldCount); // or stop directly at first failure?

    // iterate further while the stack does not change
    do {
      // print successfully read value
      std::cout << entry.count << " ";

      if (entry.next == nullptr) {
        std::cout << std::endl;
        return true; // successfully read all entries
      }
      std::memcpy(&entry, entry.next, sizeof(stack_entry));
    } while (count == oldCount);

    // if the count changes we consider the memcpy as failed and do not
    // process further entries
    std::cout << "\nconcurrent_iterate incomplete - entries changed"
              << std::endl;

    return false;
  }

  void clear() {
    // TODO -> needs deallocation (but we handle this externally for technical
    // reasons)
    top = nullptr;
  }

private:
  // we want to read the stack from another thread (peek)
  // atomic needed if we sync with count?
  std::atomic<stack_entry *> top{nullptr};
  std::atomic<uint64_t> count{0};
};

} // namespace monitor
