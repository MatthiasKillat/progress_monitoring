#pragma once

#include <chrono>

namespace monitor {

using time_unit_t = std::chrono::milliseconds;
using clock_t = std::chrono::steady_clock;
using time_t = uint64_t;

template <typename T> inline time_t toTimeUnit(const T &timePoint) {
  auto unit = std::chrono::time_point_cast<time_unit_t>(timePoint);
  return unit.time_since_epoch().count();
}

// checking is tricky due to overflow etc.
inline bool isExceeded(uint64_t deadline, uint64_t now, uint64_t &delta) {
  delta = now - deadline;
  // note that this way we can only deal with differences < max uint / 2
  // cast on unsigned int is not portable for large values (highest bit set)
  // use bitmask instead
  auto d = static_cast<int64_t>(delta);
  return d > 0; // or >=, depends on intention
}

} // namespace monitor
