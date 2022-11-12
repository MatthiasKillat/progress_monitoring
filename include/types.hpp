#pragma once

#include <chrono>
#include <limits>

namespace monitor {

using time_unit_t = std::chrono::nanoseconds;
using clock_t = std::chrono::steady_clock;
using time_point_t = std::chrono::time_point<clock_t, clock_t::duration>;
using time_t = uint64_t;
using stime_t = int64_t;

using checkpoint_id_t = uint64_t;

} // namespace monitor