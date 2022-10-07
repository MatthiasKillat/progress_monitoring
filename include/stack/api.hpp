#pragma once

#include "allocator.hpp"
#include "entry.hpp"
#include "stack.hpp"

#include <iostream>

namespace monitor {

namespace api {

inline void push(value_t value, stack &s, stack_allocator &a) {

  auto *entry = a.allocate();

  // can also assume it never fails and crash if it does
  if (!entry) {
    std::cerr << "Monitoring allocation error" << std::endl;
    std::terminate();
  }

  entry->value = value;

  s.push(*entry);
}

inline stack_entry *pop(stack &s) { return s.pop(); }

inline void free(stack_entry *entry, stack_allocator &a) {
  a.deallocate(entry);
}

thread_local monitor::stack_allocator tl_allocator;
thread_local monitor::stack tl_stack;

inline void tl_push(value_t value) {
  std::cout << "tl_push " << value << std::endl;
  std::cout << "before: ";
  tl_stack.print();
  push(value, tl_stack, tl_allocator);

  std::cout << "after: ";
  tl_stack.print();
}

inline void tl_pop() {
  std::cout << "tl_pop" << std::endl;

  std::cout << "before: ";
  tl_stack.print();

  auto entry = pop(tl_stack);
  std::cout << "after: ";
  tl_stack.print();
  free(entry, tl_allocator);
}

inline void tl_iterate() {
  std::cout << "tl_iterate" << std::endl;
  tl_stack.concurrent_iterate();
}

} // namespace api
} // namespace monitor