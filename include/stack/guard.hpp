#pragma once

#include "stack.hpp"

#include <iostream>

namespace monitor {

// will require that start and end happen in the same scope
class stack_guard {
public:
  stack_guard(stack &s, uint64_t value = 0) : m_stack(s) {
    m_entry.value = value;
    m_stack.push(m_entry);
    std::cout << "push ";
    m_stack.print();
  }

  ~stack_guard() {
    m_stack.pop();
    std::cout << "pop ";
    m_stack.print();
  }

private:
  stack_entry m_entry;
  stack &m_stack;
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

} // namespace monitor