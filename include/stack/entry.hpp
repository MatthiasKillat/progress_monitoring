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
  std::atomic<time_t> deadline{0};
  // only if both are the same, the deadline is valid
  std::atomic<time_t> deadline_validator{1};
  time_point_t start;

  bool is_valid(time_t assumed_deadline) {
    // a change of deadline can be tolerated by the algorithm (TODO: proof)
    return deadline == assumed_deadline &&
           deadline_validator == assumed_deadline;
  }
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
