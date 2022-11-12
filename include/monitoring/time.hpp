#pragma once

#include "types.hpp"

#include <chrono>

namespace monitor {

template <typename T> inline utime_t to_time_unit(const T &timePoint) {
  auto unit = std::chrono::time_point_cast<utime_unit_t>(timePoint);
  return (utime_t)unit.time_since_epoch().count();
}

inline time_t to_deadline(time_unit_t timeout) {
  auto now = clock_t::now();
  auto deadline = now + timeout;
  return to_time_unit(deadline);
}

// compare now and the deadline and compute the delta
// only works if the absolute difference is not too large; < max of uint64_t /
// 2
inline bool is_exceeded(uint64_t deadline, uint64_t now, uint64_t &delta) {
  delta = now - deadline;
  // note that this way we ca n only deal with differences < max uint / 2
  // cast on unsigned int is not portable for large values (highest bit set)
  // use bitmask instead
  auto d = static_cast<int64_t>(delta);
  return d > 0;
}

} // namespace monitor
