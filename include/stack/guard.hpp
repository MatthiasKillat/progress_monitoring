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

} // namespace monitor