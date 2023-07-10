#pragma once

#include <atomic>
#include <linux/futex.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>

struct WaitState {
  using count_t = uint64_t;
  using value_t = int; // because futex needs it
  using atomic_t = std::atomic<value_t>;

  // if this holds we can safely convert pointers (atomic has standard layout)
  static_assert(sizeof(value_t) == sizeof(atomic_t),
                "Cannot convert atomic to futex word type");

  static constexpr value_t WAITING{0};
  // anything else is non-waiting

  value_t *address() {
    // only safe if static assert holds (due to standard layout)
    // portable to all platforms where it holds
    return reinterpret_cast<value_t *>(&m_value);
  }

  value_t value() { return m_value.load(std::memory_order_relaxed); }

  value_t exchange(value_t value) {
    return m_value.exchange(WaitState::WAITING,
                            std::memory_order::memory_order_acq_rel);
  }

  bool compare_exchange(value_t &exp, value_t value) {
    return m_value.compare_exchange_strong(
        exp, value, std::memory_order_acq_rel, std::memory_order_relaxed);
  }

  count_t count() { return m_count.load(std::memory_order_relaxed); }

  count_t increment() {
    return m_count.fetch_add(1, std::memory_order::memory_order_relaxed);
  }

private:
  atomic_t m_value{WAITING};
  std::atomic<count_t> m_count{0};
};

/// @brief Behaves like an AutoResetEvent
class SingleWait {
public:
  using signal_t = WaitState::value_t;
  using count_t = WaitState::count_t;

  SingleWait(WaitState &state) : m_state(&state) {}

  signal_t wait() {

    do {
      auto value = m_state->exchange(WaitState::WAITING);
      if (value != WaitState::WAITING) {
        return value;
      }
      sleep_if_state_equals(WaitState::WAITING);
    } while (true);
  }

  count_t count() { return m_state->count(); }

private:
  WaitState *m_state{nullptr};

  void sleep_if_state_equals(signal_t value) {
    syscall(SYS_futex, m_state->address(), FUTEX_WAIT, value, 0, 0, 0);
  }
};

class Notifier {
public:
  using signal_t = WaitState::value_t;

  Notifier(WaitState &target) : m_state(&target) {}

  void notify(signal_t signal = 1) {

    // signal should not be WaitState::WAITING otherwise the waiting party
    // will continue waiting

    auto value = m_state->value();
    // keep the old bits if any (we cannot just use store as the waiting party
    // may reset any time)
    do {
    } while (!m_state->compare_exchange(value, value | signal));

    m_state->increment();

    wake_one();
  }

  bool is_waiting() { return m_state->value() == WaitState::WAITING; }

private:
  WaitState *m_state{nullptr};

  void wake_one() {
    syscall(SYS_futex, m_state->address(), FUTEX_WAKE, 1, 0, 0, 0);
  }
};
