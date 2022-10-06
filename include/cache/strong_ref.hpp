#pragma once

#include "control_block.hpp"

template <class T> class weak_ref;

// TODO: consider not allowing it to be null but then
// weak_ref has to return a pointer or similar (as it can fail to provide a
// valid strong_ref)
//
// the contract is basically: if strong_ref is valid and not invalidated,
// the it can be safely accessed
template <typename T> class strong_ref {
  friend class weak_ref<T>;
  using block_t = control_block<T>;

public:
  // created only by weak_ref
  // movable (since it helps if no weak_refs and no_strong_refs imply there
  // cannot be further refs)
  strong_ref(strong_ref &&other) : m_block(other.m_block) {
    other.m_block = nullptr;
  }

  ~strong_ref() {
    if (valid()) {
      unref();
    }
  }

  T *get() { return &m_block->value; }

  T &operator*() { return m_block->value; }

  bool valid() { return m_block != nullptr; }
  operator bool() { return valid(); }

  void invalidate() {
    if (valid()) {
      unref();
      m_block = nullptr;
    }
  }

  // debug
  void print() {
    if (!valid()) {
      std::cout << "strong nullref" << std::endl;
      return;
    }

    std::cout << "strong ref i " << m_block->index << " s = " << m_block->strong
              << " w = " << m_block->weak << " aba = " << m_block->aba
              << std::endl;
  }

private:
  block_t *m_block{nullptr};

  strong_ref() = default;
  strong_ref(block_t *block) : m_block(block) {}

  // note: copy is forbidden for simplicity, only create via weak_ref
  // TODO: relax?

  // only required by weak_ref, but not actually called (RVO)
  strong_ref(strong_ref &other) : m_block(other.m_block) {
    other.m_block = nullptr; // similar to move, RVO
  }

  void unref() {
    auto s = --m_block->strong;
    // if s == 2 no strong refs exist (for now)
    // if in addition there are no weak refs, there is a chance
    // that all refs are gone and we invoke the deleter (which will use CAS
    // still to ensure we only reclaim if there are really no strong refs
    if (s == UNREFERENCED && m_block->weak == 0) {
      // there cannot be more weak_refs later (as weak_refs are all gone
      // and strong_refs cannot create them)
      // any new weak_ref created by the weak_cache has a different aba
      m_block->invoke_deleter();
    }
  }
};