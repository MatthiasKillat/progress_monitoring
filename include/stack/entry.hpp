#pragma once

#include <atomic>
#include <stdint.h>

#include "monitoring/source_location.hpp"
#include "time.hpp"

namespace monitor {

// must be memcpyable
// we deliberately avoid generics and encapsulation here and strive for
// efficiency (it is not exposed to the user)

using checkpoint_id_t = uint64_t;

struct checkpoint {
  source_location location{nullptr};
  checkpoint_id_t id;
  std::atomic<time_t> deadline{0};
};

// the stack payload
using value_t = uint64_t;

struct stack_entry {
  stack_entry() = default;

  checkpoint check;

  uint64_t count{0};
  stack_entry *next;
};

} // namespace monitor
