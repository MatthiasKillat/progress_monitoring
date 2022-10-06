#pragma once

// need only some definitions?
#include "control_block.hpp"

#include <array>
#include <atomic>
#include <deque>
#include <optional>

// very problem specific, needs to be lock-free later
// essentially we want to build an approximate LRU cache, lock-free
template <index_t Capacity> class counting_queue {

public:
  void decrease(index_t index) {
    auto &c = m_count[index];
    c.fetch_sub(1); // will be > 0 before by usage contract
  }

  index_t push(index_t index) {
    auto &c = m_count[index];
    auto value = c.fetch_add(1);
    if (value == 0) {
      m_deque.push_back(index);
    }
    return value; // > 0 means pushed (and we know the old value)
  }

  // quite the obscure and likely wrong construction ... -.-
  std::optional<index_t> pop() {
    while (true) {
    retry:
      if (m_deque.empty()) {
        break;
      }
      // solving this lock-free will be tricky
      index_t index = m_deque.front();
      m_deque.pop_front();
      auto &c = m_count[index];
      auto value = c.load();

      // concurrent decrease may happen (up to 0) TODO problem - concurrent push
      // at 0
      while (value > 0) {
        if (c.compare_exchange_strong(value, value - 1)) {
          if (value > 1) {
            // TODO: we need to ensure to not store the same index twice
            // we decreased to non-zero, repush
            // is this safe with concurrent decrease?
            m_deque.push_back(index);
            goto retry; // try the next elements
          } else {
            // we decreased to zero
            return std::optional<index_t>(index);
          }
        }
      }
      // someone decreased to zero
      return std::optional<index_t>(index);
    }
    return std::nullopt;
  }

  size_t size() { return m_deque.size(); }

private:
  std::deque<index_t> m_deque;
  std::array<std::atomic<index_t>, Capacity> m_count{0};
};