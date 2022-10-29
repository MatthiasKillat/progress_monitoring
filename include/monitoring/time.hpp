#pragma once

#include <chrono>
#include <limits>

namespace monitor {

using time_unit_t = std::chrono::nanoseconds;
using clock_t = std::chrono::steady_clock;
using time_point_t = std::chrono::time_point<clock_t, clock_t::duration>;
using time_t = uint64_t;

using rep_t = uint64_t;
using utime_unit_t = std::chrono::duration<rep_t, std::ratio<1, 1000000000>>;
using uduration_t = std::chrono::duration<rep_t, std::ratio<1>>;
using utime_point_t = std::chrono::time_point<clock_t, uduration_t>;
using utime_t = uint64_t;

// TODO: cleanup wrt. unsigned up to initial signed clock

utime_point_t unow() {
  // TODO: internal now result can still overflow and signed overflow is UB
  auto t = std::chrono::time_point_cast<uduration_t>(clock_t::now());
  return t;
}

template <typename T> inline utime_t to_time_unit(const T &timePoint) {
  auto unit = std::chrono::time_point_cast<utime_unit_t>(timePoint);
  return (utime_t)unit.time_since_epoch().count() +
         std::numeric_limits<time_t>::max() / 2;
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
  // note that this way we can only deal with differences < max uint / 2
  // cast on unsigned int is not portable for large values (highest bit set)
  // use bitmask instead
  auto d = static_cast<int64_t>(delta);
  return d > 0;
}

} // namespace monitor
