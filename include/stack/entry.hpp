#pragma once

#include <stdint.h>

namespace monitor {

// must be memcpyable
// we deliberately avoid generics and encapsulation here and strive for
// efficiency (it is not exposed to the user)

// the stack payload
using value_t = uint64_t;

struct stack_entry {
  stack_entry() = default;

  value_t value;
  uint64_t count{0};
  stack_entry *next;
};

} // namespace monitor
