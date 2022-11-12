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

using checkpoint_id_t = uint64_t;

// must be fixed if we consider overflow mode at least, 0 is actually valid
constexpr time_t INVALID_TIME{0};

} // namespace monitor