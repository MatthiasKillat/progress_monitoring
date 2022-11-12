#pragma once

#include <atomic>
#include <stdint.h>

#include "monitoring/source_location.hpp"
#include "types.hpp"

namespace monitor {

// must be memcpyable
// we deliberately avoid generics and encapsulation here and strive for
// efficiency (it is not exposed to the user)

struct checkpoint {
  source_location location;
  checkpoint_id_t id;
  std::atomic<time_t> deadline{INVALID_TIME};
  time_point_t start;
};

// the stack payload
using value_t = uint64_t;

// no dtor needed
struct stack_entry {
  stack_entry() = default;

  checkpoint data;

  uint64_t count{0};
  stack_entry *next;
};

} // namespace monitor
