#pragma once

#include <atomic>
#include <functional>

using ref_t = uint64_t;
using count_t = uint64_t;
using index_t = uint32_t;

template <typename T> struct control_block {
  std::aligned_storage_t<sizeof(T), alignof(T)> storage;
  std::atomic<ref_t> strong{1};
  std::atomic<ref_t> weak{0};
  std::atomic<count_t> aba{0};
  std::atomic<index_t> index{0};

  // note that the deleter is shared across all blocks from the same cache
  std::function<void(control_block<T> &)> *deleter{
      nullptr}; // needed to e.g. return block to unused cache

  // strong = 0 : unused
  // strong = 1 : cache exclusive ownership, no change of strong from any ref
  // strong = 2 : unused if weak = 0

  bool try_strong_ref() {
    // c > 1 -> c + 1
    auto old = strong.load();
    while (old > 1) {
      if (strong.compare_exchange_strong(old, old + 1)) {
        return true;
      }
    }
    return false;
  }

  void strong_unref() {
    --strong; // assume locked before
  }

  // TODO: prove correctness and optimize
  bool claim() {
    // c == 0 -> 1
    ref_t exp = 0;
    if (strong.compare_exchange_strong(exp, 1)) {
      return true;
    }
    // c == 2 -> 1
    exp = 2; // only weak refs
    if (strong.compare_exchange_strong(exp, 1)) {
      return true;
    }
    return false;
  }

  bool reclaim() {
    // c == 2 -> 0
    ref_t exp = 2;
    if (strong.compare_exchange_strong(exp, 0)) {
      return true;
    }
    return false;
  }

  void invoke_deleter() { (*deleter)(*this); }
};
