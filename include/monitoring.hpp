#pragma once

#include <array>
#include <assert.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

#include "source_location.hpp"
#include "time.hpp"

namespace monitor {

using thread_id_t = std::thread::id;

struct ThreadState {
  // this atomic is crucial and the only frequent sync point between the thread
  // and the monitor (minimal interference)
  std::atomic<time_t> deadline{0};
  thread_id_t id{0};
  size_t index;

  // not strictly needed if we want to keep it light
  std::atomic<uint64_t> count{0};

  // TODO: error codes instead
  SourceLocation location; // TODO: tracing is costly, enable/disable

  // reset by monitor but count is still odd (only the thread itself will set it
  // to an even number)
  bool isLate();

  void executeHandler();

  template <typename H> void setHandler(const H &newHandler) {
    std::lock_guard<std::mutex> lock(handlerMutex);
    handler = newHandler;
  }

  bool tryGetLocation(SourceLocation &loc) {
    auto c = count.load(std::memory_order_acquire);

    // this read should be crash proof (pointers, ints) but we can see partial
    // reads?
    loc = location;

    // load valid?
    return count.compare_exchange_strong(c, c, std::memory_order_acq_rel,
                                         std::memory_order_acquire);
  }

  void reset();

private:
  // TODO: generalize signature
  std::function<void(void)> handler;
  std::mutex handlerMutex;
};

class ThreadMonitor {
public:
  ThreadMonitor();

  ~ThreadMonitor();

  // must work concurrently, mutex is ok if needed (is used rarely)
  ThreadState *loanState();

  // TODO implement
  void returnState(ThreadState &state);

  void start(time_unit_t interval = time_unit_t{10});

  void stop();

private:
  static constexpr size_t CAPACITY = 100; // TODO: make configurable
  static constexpr size_t NO_INDEX = CAPACITY;

  // TODO: use lock-free queue,
  // this is basically a slab allocator of states
  std::mutex m_indicesMutex;
  std::queue<size_t> m_indices;
  std::array<ThreadState, CAPACITY> m_states;

  std::atomic_bool m_isMonitoring{false};
  std::thread m_monitoringThread;

  time_unit_t m_interval{10};

  void prioritize(std::thread &thread);

  bool isUsed(const ThreadState &state);

  void checkDeadlines(const std::chrono::time_point<clock_t> &checkTime);

  void monitor();

  size_t getIndex();

  void freeIndex(size_t index);
};

// each thread has such a pointer
// the thread is monitored iff it is not null
inline thread_local ThreadState *tl_state{nullptr};

ThreadMonitor &threadMonitor();

bool startMonitoring();

template <typename Handler> bool startMonitoring(Handler &handler) {
  tl_state = threadMonitor().loanState();
  if (tl_state) {
    tl_state->setHandler(handler);
    return true;
  }
  return false;
}

template <typename Handler> bool setHandler(Handler &handler) {
  if (tl_state) {
    tl_state->setHandler(handler);
    return true;
  }
  return false;
}

/// @brief stops the monitoring of the current (i.e. calling) thread
void stopMonitoring();

/// @brief indicates whether the current thread is monitored
bool isMonitored();

/// @brief indicates that the current thread awaits to progress to the
/// next conformation checkpoint in timeout units of time
void awaitProgressIn(time_unit_t timeout);

/// @brief indicates that the current thread awaits to progress to the
/// next conformation checkpoint in timeout units of time,
/// provides the source location for additional output in the timeout case
void awaitProgressIn(time_unit_t timeout, const SourceLocation &location);

/// @brief confirms that the current thread has progressed to the next
/// checkpoint, outputs timeout message if there is a timeout detected by the
/// current thread
void confirmProgress(const SourceLocation &location);

} // namespace monitor
