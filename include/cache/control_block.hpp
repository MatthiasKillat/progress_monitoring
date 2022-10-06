#pragma once

#include <atomic>
#include <functional>

using ref_t = uint64_t;
using count_t = uint64_t;
using index_t = uint32_t;

constexpr ref_t FREE = 0; // TODO: needed?
constexpr ref_t EXCLUSIVE = 1;
constexpr ref_t UNREFERENCED = 2;

// note: not encapsulated for performance, implementation detail
template <typename T> struct control_block {
  std::aligned_storage_t<sizeof(T), alignof(T)> storage;
  std::atomic<ref_t> strong{FREE};
  std::atomic<ref_t> weak{FREE};
  std::atomic<count_t> aba{0};
  std::atomic<index_t> index{0};

  // note that the deleter is shared across all blocks from the same cache
  std::function<void(control_block<T> &)> *deleter{
      nullptr}; // needed to e.g. return block to unused cache

  bool try_strong_ref() {
    // c >= 2 (MAYBE IN USE) -> c + 1
    auto old = strong.load();
    while (old >= UNREFERENCED) {
      if (strong.compare_exchange_strong(old, old + 1)) {
        return true;
      }
    }
    return false;
  }

  void strong_unref() {
    --strong; // assume locked before
  }

  bool transition(ref_t from, ref_t to) {
    if (strong.compare_exchange_strong(from, to)) {
      return true;
    }
    return false;
  }

  bool make_exclusive() {
    if (transition(FREE, EXCLUSIVE)) {
      return true;
    }
    if (transition(UNREFERENCED, EXCLUSIVE)) {
      return true;
    }
    return false;
  }

  // can assume it always exists
  void invoke_deleter() { (*deleter)(*this); }
};
