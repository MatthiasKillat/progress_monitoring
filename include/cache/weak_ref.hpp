#pragma once

#include "control_block.hpp"
#include "strong_ref.hpp"

#include <iostream>

// weak refs can generate strong refs
// weak refs can generate weak refs
// strong refs can only generate strong refs
// note weak_ref and strong_ref are not thread_safe

template <class T, index_t Capacity> class weak_cache;

// TODO: assignment operators etc.

template <typename T> class weak_ref {
  template <typename S, index_t C> friend class weak_cache;

public:
  // created only by weak_cache
  // copyable, movable
  weak_ref(const weak_ref &other) : m_block(other.m_block), m_aba(other.m_aba) {
    if (valid()) {
      ref();
    }
  }

  weak_ref(weak_ref &&other) : m_block(other.m_block) {
    other.m_block = nullptr;
  }

  ~weak_ref() {
    if (valid()) {
      unref();
    }
  }

  strong_ref<T> get_strong_ref() {
    if (!valid()) {
      return strong_ref<T>(); // moves
    }

    // valid
    if (m_block->try_strong_ref()) {
      if (aba_check()) {
        return strong_ref<T>(m_block); // moves
      }
      m_block->strong_unref(); // moves
    }

    invalidate_unchecked();
    return strong_ref<T>();
  }

  // debug
  void print() {
    if (!valid()) {
      std::cout << "weak nullref" << std::endl;
      return;
    }
    std::cout << "weak ref i " << m_block->index << " s = " << m_block->strong
              << " w = " << m_block->weak << " aba = " << m_block->aba << "("
              << m_aba << ")" << std::endl;
  }

  bool valid() { return m_block != nullptr; }
  operator bool() { return valid(); }

  void invalidate() {
    if (valid()) {
      invalidate_unchecked();
    }
  }

  // expert API
  bool try_lock() { return m_block->try_strong_ref(); }

  // expert API
  // caution, unchecked and only to be used if this was gotten valid AND locked
  // in fact, it must be called otherwise the ref is lost (we do not want to
  // memorize that we need to call it for a special case only)
  void unlock() { m_block->strong_unref(); }

private:
  control_block<T> *m_block{nullptr};
  count_t m_aba;

  weak_ref(control_block<T> &block) : m_block(&block) {
    ref();
    m_aba = m_block->aba;
  }

  weak_ref() = default;

  bool aba_check() {
    // better/more powerful sanity check needed/easily possible?
    return m_aba == m_block->aba.load();
  }

  void ref() { ++m_block->weak; }

  void unref() {
    if (--m_block->weak == 0) {
      m_block->invoke_deleter();
    }
  }

  void invalidate_unchecked() {
    unref();
    m_block = nullptr;
  }
};
