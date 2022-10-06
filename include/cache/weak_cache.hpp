#pragma once

#include "cache.hpp"
#include "ref.hpp"

template <class T, index_t Capacity = 4> class weak_cache {

  using block = control_block<T>;

public:
  // can copy returned ref and obtain a strong one
  weak_ref<T> get_weak_ref() {
    auto block = m_cache.get();
    if (block) {
      // block.strong is 1 and only this get can change it
      // (if stalled, the block is lost)
      block->strong.store(2); // now others may try to generate strong refs
                              // again but will fail due to aba
      return weak_ref<T>(*block);
    }
    return weak_ref<T>();
  }

  // if not null, the reference is locke (i.e. cannot be deleted by cache before
  // being unlocked)
  weak_ref<T> get_locked_ref() {
    auto block = m_cache.get();
    if (block) {
      // we have an additional strong ref count
      block->strong.store(3); // now others may try to generate strong refs
                              // again but will fail due to aba
      return weak_ref<T>(*block);
    }
    return weak_ref<T>();
  }

  // cannot copy the ref
  strong_ref<T> get_strong_ref() {
    auto weak_ref = get_weak_ref();
    return weak_ref.get();
  }

  // returning is not required, will happen automatically

private:
  cache<T, Capacity> m_cache;
};

/*
TODO: describe problem that is solved in detail

Fast path (good enough?)
1. pop unused index (CAS with lock-free queue later)
2. set the block counters uncontended
3. return weak_ref that wraps the block

Slow path (bad)
1. unused is empty
2. we succeed after popping n times from maybeUnused (n-1 strongly used)
3. we must assume weak users, so we set the strong counter via CAS
4. set the remaining counters (while strong = 1, exclusive)
5. return weak_ref that wraps the block

Unsuccessful path (worst)
1. unused empty
2. all pops from maybeUnused fail (TODO: retry heuristics)
3. we give up and assume all blocks are strongly used
*/