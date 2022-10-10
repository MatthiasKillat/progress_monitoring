#pragma once

#include "entry.hpp"

#include <cstddef>
#include <list>
#include <new>
#include <queue>
#include <type_traits>

namespace monitor {

// can be made more generic later, does not need to be thread-safe
// TODO: does not have to rely on dynamic memory, but then we cannot use
// std::queue etc (can be changed but gets more cumbersome with static sizes)
// currently we do not catch potential bad_allocs anywhere
class stack_allocator {

  static constexpr size_t NUM_ENTRIES_PER_BATCH = 128;

  using batch_t =
      std::aligned_storage_t<sizeof(stack_entry), alignof(stack_entry)>;

public:
  stack_allocator() { allocate_batch(); }

  stack_allocator(const stack_allocator &) = delete;

  ~stack_allocator() {
    // only free the batches in dtor
    for (auto batch : m_batches) {
      delete[] batch;
    }
  }

  stack_entry *allocate() {
    if (m_entries.empty()) {
      // try to allocate another batch
      if (!allocate_batch()) {
        return nullptr;
      }
    }
    auto entry = m_entries.front();
    m_entries.pop();
    return entry;
  }

  void deallocate(stack_entry *entry) { m_entries.push(entry); }

private:
  std::list<batch_t *> m_batches;
  std::queue<stack_entry *> m_entries;

  bool allocate_batch() {
    auto batch = new (std::nothrow) batch_t[NUM_ENTRIES_PER_BATCH];
    if (!batch) {
      return false;
    }
    m_batches.push_back(batch);
    for (size_t i = 0; i < NUM_ENTRIES_PER_BATCH; ++i) {
      m_entries.push(reinterpret_cast<stack_entry *>(&batch[i]));
    }

    return true;
  }
};

} // namespace monitor