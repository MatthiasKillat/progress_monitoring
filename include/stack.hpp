#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <type_traits>

// must be memcpyable
struct StackEntry {
  StackEntry() = default;

  uint64_t value;
  uint64_t count{0};
  StackEntry *next;
};

// only modified in one context
class Stack {
public:
  Stack() : top{nullptr} {}

  void push(StackEntry &entry) {
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

  StackEntry *pop() {
    auto p = top.load();
    if (!p) {
      return nullptr;
    }
    top.store(p->next);
    return top;
  }

  void print() {
    auto p = top.load();
    while (p) {
      std::cout << p->count << " (" << p->value << ") ";
      p = p->next;
    }
    std::cout << "eos" << std::endl;
  }

  // using optional would require an additional copy
  bool peek(StackEntry &result) {

    StackEntry *t = top.load();
    while (t) {
      // TODO: enforce order with fences
      auto oldCount = count.load(std::memory_order_acquire);

      // top may change but memcpy will succeed (potentially with garbage)
      std::memcpy(&result, &t, sizeof(StackEntry));

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

    StackEntry entry;
    StackEntry *t;
    uint64_t oldCount;

    std::cout << "concurrent_iterate ";

    do {
      t = top.load();
      if (!t) {
        std::cout << std::endl;
        return true; // no value, i.e. we read them all
      }

      oldCount = count.load(std::memory_order_acquire);

      std::memcpy(&entry, t, sizeof(StackEntry));

    } while (count != oldCount); // or stop directly at first failure?

    // iterate further while the stack does not change
    do {
      // print successfully read value
      std::cout << entry.count << " (" << entry.value << ") ";

      if (entry.next == nullptr) {
        std::cout << std::endl;
        return true; // successfully read all entries
      }
      std::memcpy(&entry, entry.next, sizeof(StackEntry));
    } while (count == oldCount);

    // if the count changes we consider the memcpy as failed and do not process
    // further entries

    std::cout << std::endl;
    return false;
  }

private:
  // we want to read the stack from another thread (peek)
  // atomic needed if we sync with count?
  std::atomic<StackEntry *> top{nullptr};
  std::atomic<uint64_t> count{0};
};

// will require that start and end happen in the same scope
class StackGuard {
public:
  StackGuard(Stack &stack, uint64_t value = 0) : m_stack(stack) {
    m_entry.value = value;
    m_stack.push(m_entry);
    std::cout << "push ";
    m_stack.print();
  }

  ~StackGuard() {
    m_stack.pop();
    std::cout << "pop ";
    m_stack.print();
  }

private:
  StackEntry m_entry;
  Stack &m_stack;
};

template <uint32_t N, uint32_t C = 1> class Cache {
  // by the time we wrap around t actually add 0 as a value the cache contains
  // other entries
  static constexpr uint32_t INITIAL_ENTRY = 0;

public:
  // no duplicate checks for efficiency

  void add(uint64_t value) {
    auto i = value % N;
    auto n = next[i];
    data[i][n] = value; // TODO: do not allow null values
    next[i] = (n + 1) % C;
  }

  // we never remove but only overwrite the latest per line

  bool find(uint64_t value) {
    auto i = value % N;
    auto &line = data[i];
    for (uint32_t j = 0; j < C; ++j) {
      if (line[j] == value) {
        return true;
      }
    }
    return false;
  }

private:
  std::array<std::array<uint64_t, C>, N> data{
      std::array<uint64_t, C>{INITIAL_ENTRY}};
  std::array<uint32_t, N> next{0};
};