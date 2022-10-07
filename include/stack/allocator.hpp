#pragma once

#include "entry.hpp"

namespace monitor {

// can be more generic later
class stack_allocator {
public:
  // can use hybrid storage (automatic + dynamic) or similar later

  stack_entry *allocate() { return new stack_entry; }

  void deallocate(stack_entry *entry) { delete entry; }
};

} // namespace monitor