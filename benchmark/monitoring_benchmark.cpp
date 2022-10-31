#include <benchmark/benchmark.h>

// turn all monitoring macros into noops,
// hence we should see the same time as in a do nothing benchmark
// #define MONITORING_OFF

#include "monitoring/api.hpp"
#include "monitoring/macros.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace {
using namespace std::chrono_literals;

thread_local bool tl_deadline_violation{false};

void handler(monitor::checkpoint &) { tl_deadline_violation = true; }

inline void busy_loop(std::chrono::nanoseconds time) {

  auto timeout = monitor::clock_t::now() + time;
  while (timeout > monitor::clock_t::now())
    ;
}

// ensure that it is optimized out (inline does not suffice, there is some
// overhead due to call or argument construction - reason not checked yet)
#ifdef MONITORING_OFF
#define BUSY_LOOP(time)
#else
#define BUSY_LOOP(time) busy_loop(time);
#endif

class BM_Monitoring : public benchmark::Fixture {
public:
  void SetUp(const ::benchmark::State &state) {
    tl_deadline_violation = false;
    START_THIS_THREAD_MONITORING;
    SET_MONITORING_HANDLER(handler);
  }

  void TearDown(const ::benchmark::State &state) {
    STOP_THIS_THREAD_MONITORING;
  }
};

BENCHMARK_F(BM_Monitoring, SingleDeadline)(benchmark::State &state) {
  for (auto _ : state) {
    EXPECT_PROGRESS_IN(100ms, 1);

    CONFIRM_PROGRESS;
  }
  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_Monitoring, DoubleDeadline)(benchmark::State &state) {
  for (auto _ : state) {
    EXPECT_PROGRESS_IN(1000ms, 1);
    EXPECT_PROGRESS_IN(100ms, 2);

    CONFIRM_PROGRESS;
    CONFIRM_PROGRESS;
  }

  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_Monitoring, TripleDeadline)(benchmark::State &state) {
  for (auto _ : state) {
    EXPECT_PROGRESS_IN(10000ms, 1);
    EXPECT_PROGRESS_IN(1000ms, 2);
    EXPECT_PROGRESS_IN(100ms, 3);

    CONFIRM_PROGRESS;
    CONFIRM_PROGRESS;
    CONFIRM_PROGRESS;
  }

  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_Monitoring, SingleDeadlineGuard)(benchmark::State &state) {
  for (auto _ : state) {
    EXPECT_SCOPE_END_REACHED_IN(100ms, 1);
  }

  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_Monitoring, DoubleDeadlineGuard)(benchmark::State &state) {
  for (auto _ : state) {
    EXPECT_SCOPE_END_REACHED_IN(100ms, 1);
    EXPECT_SCOPE_END_REACHED_IN(10ms, 2);
  }

  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_Monitoring, SingleDeadlineViolation)(benchmark::State &state) {
  for (auto _ : state) {
    EXPECT_PROGRESS_IN(1ns, 1);
    BUSY_LOOP(100ns);
    CONFIRM_PROGRESS;
  }
  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_Monitoring, DoubleDeadlineViolation)(benchmark::State &state) {
  for (auto _ : state) {
    EXPECT_PROGRESS_IN(2ns, 1);
    EXPECT_PROGRESS_IN(1ns, 1);
    BUSY_LOOP(100ns);
    CONFIRM_PROGRESS;
    CONFIRM_PROGRESS;
  }
  benchmark::ClobberMemory();
}

//***************
//*Parameterized*
//***************

static void BM_P_MultiDeadline(benchmark::State &state) {
  auto n = state.range(0);
  tl_deadline_violation = false;
  START_THIS_THREAD_MONITORING;
  SET_MONITORING_HANDLER(handler);

  for (auto _ : state) {
    // ignore slight overhead of the loop
    for (int i = 0; i < n; ++i) {
      EXPECT_PROGRESS_IN(100ms, i);
      CONFIRM_PROGRESS;
    }
  }

  benchmark::ClobberMemory();
  STOP_THIS_THREAD_MONITORING;
}

// BENCHMARK(BM_P_MultiDeadline)->RangeMultiplier(2)->Range(1, 128);
BENCHMARK(BM_P_MultiDeadline)->DenseRange(1, 8);

static void BM_P_MultiDeadlineViolation(benchmark::State &state) {
  auto n = state.range(0);
  tl_deadline_violation = false;
  START_THIS_THREAD_MONITORING;
  SET_MONITORING_HANDLER(handler);

  for (auto _ : state) {
    // ignore slight overhead of the loop
    for (int i = 0; i < n; ++i) {
      EXPECT_PROGRESS_IN(1ns, i);
      std::this_thread::sleep_for(1ns);
      CONFIRM_PROGRESS;
    }
  }
  benchmark::ClobberMemory();
  STOP_THIS_THREAD_MONITORING;
}

// BENCHMARK(BM_P_MultiDeadlineViolation)->RangeMultiplier(2)->Range(1, 128);
BENCHMARK(BM_P_MultiDeadlineViolation)->DenseRange(1, 8);

//***************
//*Multithreaded*
//***************

static void BM_MT_SingleDeadline(benchmark::State &state) {
  tl_deadline_violation = false;
  START_THIS_THREAD_MONITORING;
  SET_MONITORING_HANDLER(handler);

  for (auto _ : state) {
    EXPECT_PROGRESS_IN(1000ms, 1);
    // std::this_thread::sleep_for(1000ns);
    CONFIRM_PROGRESS;
  }

  benchmark::ClobberMemory();
  STOP_THIS_THREAD_MONITORING;
}

BENCHMARK(BM_MT_SingleDeadline)->ThreadRange(1, 128)->UseRealTime();

static void BM_MT_SingleDeadlineViolation(benchmark::State &state) {
  tl_deadline_violation = false;
  START_THIS_THREAD_MONITORING;
  SET_MONITORING_HANDLER(handler);

  for (auto _ : state) {
    EXPECT_PROGRESS_IN(1ns, 1);
    BUSY_LOOP(100ns);
    CONFIRM_PROGRESS;
  }

  benchmark::ClobberMemory();
  STOP_THIS_THREAD_MONITORING;
}

BENCHMARK(BM_MT_SingleDeadlineViolation)->ThreadRange(1, 128)->UseRealTime();

} // namespace

int main(int argc, char **argv) {
  char arg0_default[] = "benchmark";
  char *args_default = arg0_default;
  if (!argv) {
    argc = 1;
    argv = &args_default;
  }
  START_ACTIVE_MONITORING(100ms);

  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();

  STOP_ACTIVE_MONITORING;
  return 0;
}
