#pragma once

#include <array>
#include <atomic>
#include <deque>
#include <mutex>

#include <iostream>
#include <type_traits>

#include "control_block.hpp"

template <class T, index_t Capacity> class cache {
  static constexpr index_t DEFAULT_INDEX = 0;

  using block = control_block<T>;

public:
  cache() {
    m_deleter = [this](block &b) {
      std::cout << "deleter called" << std::endl;
      this->ret(b);
    };

    for (index_t i = 0; i < Capacity; ++i) {
      auto &b = m_slots[i];
      b.index = i;
      b.deleter = &m_deleter;
      m_unused.push_back(i);
    }
  }

  block *get() {
    std::lock_guard<std::mutex> guard(m_mut); // TODO: no mutex later
    index_t i;

    do {
      if (!m_unused.empty()) {
        i = m_unused.front();
        m_unused.pop_front();

        auto &b = get(i);
        claim_uncontended(b);
        m_maybeUsed.push_back(i);
        return &b;
      }

      auto n = m_maybeUsed.size();

      for (uint32_t i = 0; i < n; ++i) {
        if (!m_maybeUsed.empty()) {
          i = m_maybeUsed.front();
          m_maybeUsed.pop_front();

          auto &b = get(i);
          if (claim_contended(b)) { // may fail if used or exclusive
            m_maybeUsed.push_back(i);
            return &b;
          } else {
            // it is now at the end of queue again
            m_maybeUsed.push_back(i);
          }
        }
      }
      // todo: stop after some time if we were unsuccessful
      // HEURISTICS: if we encountered an exclusive item block
      // we may try again (as dould now be in unused)
      break;
    } while (true);

    return nullptr;
  }

  void ret(block &block) {

    if (block.weak == 0) {
      // no further weak refs
      if (block.transition(UNREFERENCED, EXCLUSIVE)) {
        // now it can be in both queues but cannot be used
        // from the maybeUSed queue anymore (CAS will fail)
        std::cout << "***" << std::endl;
        if (block.weak == 0) { // required check?
          std::lock_guard<std::mutex> guard(m_mut);
          std::cout << "moved to unused " << block.index << std::endl;
          m_unused.push_back(block.index);
        } else {
          // still in maybeUsed
        }
      }
    }
  }

private:
  std::mutex m_mut;
  std::array<block, Capacity> m_slots;
  std::deque<index_t> m_maybeUsed;
  std::deque<index_t> m_unused; // not needed to be a deque later
  std::atomic<count_t> m_count{73};

  std::function<void(block &)> m_deleter;

  block &get(index_t i) { return m_slots[i]; }

  count_t update_count() { return m_count.fetch_add(1); }

  void claim_uncontended(block &b) {
    // we have exclusive access
    b.aba = update_count();
  }

  bool claim_contended(block &b) {
    if (!b.make_exclusive()) {
      return false;
    }
    // now we know strong = 1, weak = ?
    // no strong refs can be created by others (requires strong >= 2)

    b.aba = update_count(); // will invalidate old weak refs
    return true;
  }
};
