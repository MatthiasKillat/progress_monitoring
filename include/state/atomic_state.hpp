#include <atomic>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <iostream>

namespace traits {

// std::is_trivially_copyable is not the right trait because it precludes
// T to have e.g. dtors, therefore use a opt-in approach with a custom trait
template <typename T> struct is_memcopyable : public std::false_type {};

} // namespace traits

/// @brief single writer atomic state
template <typename T> class sw_atomic_state {
private:
  static constexpr size_t SIZE = sizeof(T);
  static constexpr size_t ALIGN = alignof(T);

  // actually the copy ctor is allowed to exist but must be equivalent to a
  // memcpy
  static_assert(traits::is_memcopyable<T>::value, "T must be memcopyable");

public:
  using storage_t = typename std::aligned_storage<SIZE, ALIGN>::type;
  using count_t = uint64_t;

  template <typename... Args> sw_atomic_state(Args &&...args);

  ~sw_atomic_state();

  sw_atomic_state(const sw_atomic_state &) = delete;
  sw_atomic_state(sw_atomic_state &&) = delete;
  sw_atomic_state &operator=(const sw_atomic_state &) = delete;
  sw_atomic_state &operator=(sw_atomic_state &&) = delete;

  /// @brief set the current value (in-place construction with args)
  /// @note caller must ensure there are no concurrent writes!
  template <typename... Args> void set(Args &&...args);

  /// @brief access the current value
  /// @note caller must ensure there are no concurrent writes!
  /// @note if there are concurrent loads, any changes to the referenced data
  /// must be atomic
  T &get();

  /// @brief try to load the current value and copy into destination,
  /// @note fails if there are concurrent writes
  bool try_load(storage_t *dest);

  /// @brief load the current value and copy into destination,
  /// @note repeats until successful
  void load(storage_t *dest);

  /// @brief load the current value and return it
  /// @note may generate an additional copy (depends on NVO)
  T load();

  /// @brief return the monotonic counter
  /// @note counter is monotonic but will wrap around, use only for equality
  /// comparison or the has_changed() function
  count_t count();

  /// @brief Checks whether the counter is equal to the provided counter
  /// @note Will internally force a strong synchronization but even after this
  /// call the result may be outdated (i.e. the state changed again),
  /// Main use: if the check returns true, as then the state has certainly
  /// changed
  bool has_changed(count_t count);

  /// @brief resets and unlocks any writers, should only be called
  /// if there are no concurrent writes in progress (it will not check)
  /// @note: purpose is to make the data structure ready for writes again after
  /// crash without losing the content
  void reset();

  /// @brief call after update by reference
  void update() { m_count.fetch_add(2, std::memory_order_release); }

private:
  storage_t m_data[2];

  std::atomic<size_t> m_current{0};
  std::atomic<count_t> m_count{0};

  size_t current() { return m_current.load(std::memory_order_relaxed); }

  size_t other() { return (m_current.load(std::memory_order_relaxed) + 1) % 2; }

  T *current_ptr() { return reinterpret_cast<T *>(&m_data[current()]); }

  T *other_ptr() { return reinterpret_cast<T *>(&m_data[other()]); }

  // TODO: weaken fences where possible
  inline void full_fence() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
  }

  count_t sync_count() {
    count_t count{0};
    // force the update of count
    m_count.compare_exchange_strong(count, count, std::memory_order_relaxed,
                                    std::memory_order_relaxed);
    return count;
  }
};

template <typename T>
template <typename... Args>
sw_atomic_state<T>::sw_atomic_state(Args &&...args) {
  auto p = current_ptr();
  new (p) T(std::forward<Args>(args)...);
}

template <typename T> sw_atomic_state<T>::~sw_atomic_state() {
  auto p = current_ptr();
  p->~T();
}
template <typename T>
template <typename... Args>
void sw_atomic_state<T>::set(Args &&...args) {

  // load starts memcpy here -> will be invalidated if it ends after increment;
  // otherwise it succeeds

  // this counter change is required to detect any concurrent read of the same
  // current buffer
  // the buffer may change and the content destroyed and possibly constructed
  // again while a potential reader performs a memcpy (this will never work
  // with a copy ctor without ownership transfer and exclusive write)
  m_count.fetch_add(1, std::memory_order_acq_rel);

  // count is odd

  // load starts memcpy here -> invalid as count is odd

  auto p1 = current_ptr();
  auto p2 = other_ptr();
  new (p2) T(std::forward<Args>(args)...);

  full_fence();

  // no one else is writing to m_current at this time (not checked!)
  // swap buffers (0->1 or 1->0)
  m_current.store((m_current.load() + 1) % 2, std::memory_order_relaxed);

  // load starts memcpy here -> invalid as count is odd

  full_fence();

  p1->~T(); // must happen after the buffer swap and before the second increment

  // must happen after the update (hence the fence)
  m_count.fetch_add(1, std::memory_order_acq_rel);
  // count is even

  // load starts memcpy here -> succeeds unless there is another set
  // incrementing the count
}

template <typename T> T &sw_atomic_state<T>::get() { return *current_ptr(); }

template <typename T>
bool sw_atomic_state<T>::try_load(sw_atomic_state<T>::storage_t *dest) {
  auto prev = count();
  if (prev % 2 != 0) {
    // there may be a concurrent write
    return false;
  }

  // old count c is even, so there was no write at the time of count()
  // but it is possible that one happens in the meantime; will be detected by
  // counter change

  auto p = current_ptr();
  memcpy(dest, p, SIZE);

  // did the count change?
  // NB: we can never detect a wrap-around but this is quite unlikely to
  // happen in any realistic amount of time (with a 64 bit counter)
  return !has_changed(prev);
}

template <typename T>
void sw_atomic_state<T>::load(sw_atomic_state<T>::storage_t *dest) {
  while (!try_load(dest))
    ;
}

template <typename T> T sw_atomic_state<T>::load() {
  storage_t dest;
  load(&dest);
  return *reinterpret_cast<T *>(&dest);
}

template <typename T>
typename sw_atomic_state<T>::count_t sw_atomic_state<T>::count() {
  return m_count.load(std::memory_order_relaxed);
}

template <typename T> bool sw_atomic_state<T>::has_changed(count_t count) {
  return count != sync_count();
}

template <typename T> void sw_atomic_state<T>::reset() { m_count = 0; }
