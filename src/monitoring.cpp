#include "monitoring.hpp"

namespace monitor {

bool ThreadState::isLate() { return deadline == 0 && count.load() % 2 == 1; }

void ThreadState::executeHandler() {
  // needed to not set the handler when it is executing
  std::lock_guard<std::mutex> lock(handlerMutex);
  if (handler) {
    handler();
  }
}

void ThreadState::reset() {
  std::lock_guard<std::mutex> lock(handlerMutex);
  handler = nullptr;
  deadline = 0;
  count = 0;
}

ThreadMonitor::ThreadMonitor() {
  for (size_t i = 0; i < CAPACITY; ++i) {
    m_indices.push(i);
  }
}

ThreadMonitor::~ThreadMonitor() { stop(); }

size_t ThreadMonitor::getIndex() {
  std::lock_guard<std::mutex> lock(m_indicesMutex);

  if (!m_indices.empty()) {
    auto index = m_indices.front();
    m_indices.pop();
    return index;
  }
  return ThreadMonitor::NO_INDEX;
}

void ThreadMonitor::freeIndex(size_t index) {
  std::lock_guard<std::mutex> lock(m_indicesMutex);
  m_indices.push(index);
}

ThreadState *ThreadMonitor::loanState() {
  auto index = getIndex();
  if (index == NO_INDEX) {
    return nullptr;
  }

  auto &state = m_states[index];
  state.index = index;
  state.id = std::this_thread::get_id();

  return &state;
}
void ThreadMonitor::returnState(ThreadState &state) {
  auto index = state.index;
  state.reset();
  freeIndex(index);
}

void ThreadMonitor::start(time_unit_t interval) {
  if (!m_isMonitoring) {
    m_isMonitoring = true;
    m_interval = interval;

    m_monitoringThread = std::thread(&ThreadMonitor::monitor, this);
    prioritize(m_monitoringThread);
  }
}

void ThreadMonitor::stop() {
  if (m_isMonitoring) {
    m_isMonitoring = false;
    m_monitoringThread.join();
  }
}

void ThreadMonitor::prioritize(std::thread &thread) {
  // NB: works only on linux for now
  auto h = thread.native_handle();
  constexpr int policy = SCHED_FIFO;
  sched_param params;
  params.sched_priority = sched_get_priority_max(policy);
  auto result = pthread_setschedparam(h, policy, &params);
  if (result != 0) {
    // requires e.g. root rights
    std::cout << "Error setting monitoring thread priority" << std::endl;
  }
}

bool ThreadMonitor::isUsed(const ThreadState &state) {
  return state.index < CAPACITY;
}

void ThreadMonitor::checkDeadlines(
    const std::chrono::time_point<clock_t> &checkTime) {
  auto now = toTimeUnit(checkTime);

  for (auto &state : m_states) {
    if (isUsed(state)) {

      uint64_t delta;
      auto deadline =
          state.deadline.load(std::memory_order_relaxed); // sufficient?

      // NB: this state may become unused in the meantime
      if (deadline > 0 && isExceeded(deadline, now, delta)) {

        if (state.deadline.compare_exchange_strong(deadline, 0,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
          // only react once (i.e. not if the thread itself already detected
          // timeout)

          // if the thread progresses in the meantime the location will be
          // invalidated (and cannot be recovered)
          SourceLocation location;

          auto validLocation = state.tryGetLocation(location);
          // current thread detects lateness (but of course no deadlock)
          // useful if we are only a little late
          std::cout << "[Monitoring Thread detected] thread id " << state.id
                    << ": deadline will be exceeded by at least " << delta
                    << " time units";
          if (validLocation) {
            std::cout << " at CONFIRM PROGRESS in " << state.location
                      << std::endl;
          } else {
            std::cout << " at CONFIRM PROGRESS in UNKNOWN LOCATION"
                      << std::endl;
          }
          state.executeHandler();
        }
      }
    }
  }
}

void ThreadMonitor::monitor() {
  uint64_t tick{1};
  while (m_isMonitoring) {
    auto now = clock_t::now();
    auto wakeup = now + m_interval;
    checkDeadlines(now);
    std::this_thread::sleep_until(wakeup);
  }
}

ThreadMonitor &threadMonitor() {
  static ThreadMonitor monitor;
  return monitor;
}

bool startMonitoring() {
  tl_state = threadMonitor().loanState();
  if (tl_state) {
    return true;
  }
  return false;
}

void stopMonitoring() {
  if (!tl_state)
    return;
  tl_state->reset();
  threadMonitor().returnState(*tl_state);
  tl_state = nullptr;
}

bool isMonitored() { return tl_state != nullptr; }

void awaitProgressIn(time_unit_t timeout) {
  if (!isMonitored) {
    return;
  }

  auto now = clock_t::now();
  auto deadline = now + timeout;
  tl_state->deadline = toTimeUnit(deadline);
  ++tl_state->count;
}

void awaitProgressIn(time_unit_t timeout, const SourceLocation &location) {
  if (!isMonitored) {
    return;
  }

  assert(tl_state->deadline.load(std::memory_order_relaxed) ==
         0); // misuse otherwise

  tl_state->location = location; // TODO: likely race, implications?

  auto now = clock_t::now();
  auto deadline = now + timeout;
  tl_state->deadline = toTimeUnit(deadline);
  ++tl_state->count;
}

void confirmProgress(const SourceLocation &location) {
  if (!isMonitored) {
    return;
  }

  if (tl_state->deadline == 0) {
    return;
  }
  auto now = toTimeUnit(clock_t::now());
  auto deadline = tl_state->deadline.load();

  assert(deadline > 0); // misuse otherwise

  uint64_t delta;
  if (isExceeded(deadline, now, delta)) {
    // current thread detect lateness (but of course no deadlock)
    // useful if we are only a little late
    std::cout << "[This Thread detected] thread id " << tl_state->id
              << ": deadline exceeded by " << delta
              << " time units at CONFIRM PROGRESS in " << location << std::endl;
    tl_state->executeHandler();
  }
  ++tl_state->count;
  tl_state->deadline.store(0);
}

} // namespace monitor