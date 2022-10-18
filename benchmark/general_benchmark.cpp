#include "monitoring/api.hpp"
#include <benchmark/benchmark.h>

#include <atomic>
#include <bits/types/clock_t.h>
#include <chrono>
#include <mutex>

namespace {
using namespace std::chrono_literals;

thread_local bool tl_deadline_violation{false};

void handler(monitor::checkpoint &) { tl_deadline_violation = true; }

inline void busy_loop(std::chrono::nanoseconds time) {

  auto timeout = monitor::clock_t::now() + time;
  while (timeout > monitor::clock_t::now())
    ;
}

class BM_General : public benchmark::Fixture {
public:
  void SetUp(const ::benchmark::State &) {}

  void TearDown(const ::benchmark::State &) {}
};

constexpr int ITERATIONS = 1000;

// some unrelated reference benchmarks, TODO: move to reference benchmark file
BENCHMARK_F(BM_General, DoNothing)(benchmark::State &state) {
  for (auto _ : state) {
  }
  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_General, AsIfRule)(benchmark::State &state) {

  int r;
  for (auto _ : state) {
    for (int i = 0; i < ITERATIONS; ++i) {
      r = i;
    }
  }
  // only write r once (last iteration),
  // without the artificial use (DoNotOptimize), even this is not required
  // and the whole loop can be optimized away
  benchmark::DoNotOptimize(r);
  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_General, VolatileWrite)(benchmark::State &state) {

  volatile int r;
  for (auto _ : state) {
    for (int i = 0; i < ITERATIONS; ++i) {
      // writes still ITERATION times, even though
      // no one has access to r (to the knowledge of the compiler)
      // the compiler must assume it might be mapped to some observable memory
      r = i;
    }
  }
  benchmark::DoNotOptimize(r);
  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_General, VolatileLoop)(benchmark::State &state) {

  for (auto _ : state) {
    for (volatile int i = 0; i < ITERATIONS; ++i) {
    };
  }
  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_General, BusyLoop)(benchmark::State &state) {

  for (auto _ : state) {
    busy_loop(10us);
  }
  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_General, LoopWithConditional)(benchmark::State &state) {

  volatile int a[2] = {73, 21};
  volatile int n = ITERATIONS;
  int r = 0;
  for (auto _ : state) {
    for (int i = 0; i < n; ++i) {
      if (i % 2 == 0) {
        r += a[0];
      } else {
        r += a[1];
      }
    }
  }
  benchmark::DoNotOptimize(r);
  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_General, LoopWithArithmetics)(benchmark::State &state) {
  volatile int a[2] = {73, 21};
  volatile int n = ITERATIONS;
  int r = 0;
  for (auto _ : state) {
    // removing volatile in both may lead to the same optimization as in
    // LoopWithConditional
    for (int i = 0; i < n; ++i) {
      // does the same as the conditional variant (but faster by avoiding
      // the conditional)
      r += a[i % 2];
    }
  }
  benchmark::DoNotOptimize(r);
  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_General, ConditionalMod3)(benchmark::State &state) {
  volatile int a[2] = {73, 21};
  int r = 0;
  for (auto _ : state) {
    for (int i = 0; i < ITERATIONS; ++i) {
      if (i % 3 < 2) {
        r += a[0];
      } else {
        r += a[1];
      }
    }
  }
  benchmark::DoNotOptimize(r);
  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_General, LoopMod3)(benchmark::State &state) {
  volatile int a[2] = {73, 21};
  int r = 0;
  for (auto _ : state) {
    for (int i = 0; i < ITERATIONS; ++i) {
      int k = (i % 3) / 2;
      r += a[k];
    }
  }
  benchmark::DoNotOptimize(r);
  benchmark::ClobberMemory();
}

BENCHMARK_F(BM_General, Timestamp)(benchmark::State &state) {
  auto time = monitor::clock_t::now();

  for (auto _ : state) {
    time = monitor::clock_t::now();
  }
  benchmark::DoNotOptimize(time);
  benchmark::ClobberMemory();
}

//***************
//*Multithreaded*
//***************

std::mutex g_mutex;
std::atomic<int> g_atomic{0};

static void BM_MT_BusyLoop(benchmark::State &state) {

  for (auto _ : state) {
    busy_loop(128us);
  }

  benchmark::ClobberMemory();
}

// 10 - 12 threads will be the point of almost optimal utilization
// on a 12 core system (there is overhead from the OS, other processes and the
// active monitoring thread)
BENCHMARK(BM_MT_BusyLoop)
    ->ThreadRange(1, 128)
    ->Threads(10)
    ->Threads(11)
    ->Threads(12)
    ->Threads(13)
    ->UseRealTime();

static void BM_MT_Mutex(benchmark::State &state) {

  for (auto _ : state) {
    g_mutex.lock();
    g_mutex.unlock();
  }

  benchmark::ClobberMemory();
}

BENCHMARK(BM_MT_Mutex)->ThreadRange(1, 128)->UseRealTime();

static void BM_MT_AtomicLoadStore(benchmark::State &state) {

  for (auto _ : state) {
    // not equivalent to fetch_add(1) in concurrent use
    auto v = g_atomic.load(std::memory_order_acquire);
    g_atomic.store(v + 1, std::memory_order_release);
  }

  benchmark::ClobberMemory();
}

BENCHMARK(BM_MT_AtomicLoadStore)->ThreadRange(1, 128)->UseRealTime();

static void BM_MT_AtomicFetchAdd(benchmark::State &state) {
  int v;
  for (auto _ : state) {
    v = g_atomic.fetch_add(1, std::memory_order_acq_rel);
  }

  benchmark::DoNotOptimize(v);
  benchmark::ClobberMemory();
}

BENCHMARK(BM_MT_AtomicFetchAdd)->ThreadRange(1, 128)->UseRealTime();

static void BM_MT_AtomicExchange(benchmark::State &state) {
  int v;
  for (auto _ : state) {
    v = g_atomic.exchange(1, std::memory_order_acq_rel);
  }

  benchmark::DoNotOptimize(v);
  benchmark::ClobberMemory();
}

BENCHMARK(BM_MT_AtomicExchange)->ThreadRange(1, 128)->UseRealTime();

static void BM_MT_AtomicCas(benchmark::State &state) {
  auto v = g_atomic.load();
  for (auto _ : state) {
    // may fail concurrently
    g_atomic.compare_exchange_strong(v, v + 1, std::memory_order_acq_rel,
                                     std::memory_order_acquire);
  }

  benchmark::ClobberMemory();
}

BENCHMARK(BM_MT_AtomicCas)->ThreadRange(1, 128)->UseRealTime();

} // namespace

int main(int argc, char **argv) {
  char arg0_default[] = "benchmark";
  char *args_default = arg0_default;
  if (!argv) {
    argc = 1;
    argv = &args_default;
  }

  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();

  return 0;
}