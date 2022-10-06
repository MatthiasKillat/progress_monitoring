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
      auto &s = m_slots[i];
      s.index = i;
      s.deleter = &m_deleter;
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

        auto &s = get(i);
        claim_uncontended(s);
        m_maybeUsed.push_back(i);
        return &s;
      }

      auto n = m_maybeUsed.size();

      for (uint32_t i = 0; i < n; ++i) {
        if (!m_maybeUsed.empty()) {
          i = m_maybeUsed.front();
          m_maybeUsed.pop_front();

          auto &s = get(i);
          if (claim_contended(s)) { // may fail if used
            m_maybeUsed.push_back(i);
            return &s;
          } else {
            // it is now at the end of queue again
            m_maybeUsed.push_back(i);
          }
        }
      }
      // todo: stop after some time if we were unsuccessful
      break;
    } while (true);

    return nullptr;
  }

  void ret(block &block) {

    if (block.weak == 0) {
      if (block.reclaim()) {
        // there can be no strong refs
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
  // 0 is a special block
  std::array<block, Capacity + 1> m_slots;
  std::deque<index_t> m_maybeUsed;
  std::deque<index_t> m_unused; // not needed to be a deque later
  std::atomic<count_t> m_count{73};

  std::function<void(block &)> m_deleter;

  block &get(index_t i) { return m_slots[i]; }

  count_t update_count() { return m_count.fetch_add(1); }

  void claim_uncontended(block &s) {
    // can assume weak == 0 == strong
    s.strong.store(1);
    s.aba = update_count();
  }

  bool claim_contended(block &s) {
    if (!s.claim()) {
      return false;
    }
    // now we know strong = 1, weak = ?
    // no strong refs can be created by others (requires strong >= 2)

    s.aba = update_count(); // will invalidate old weak refs
    return true;
  }
};
