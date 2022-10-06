#pragma once

#include "control_block.hpp"

// if the block is used, cache holds one ref
// cache gives a weak ref
// weak refs can generate strong refs
// weak refs can generate weak refs
// strong refs can only generate strong refs

// note weak_ref and strong_ref are not thread_safe
template <class T, index_t Capacity> class weak_cache;

template <class T> class weak_ref;

template <typename T> class strong_ref {
  friend class weak_ref<T>;
  using block_t = control_block<T>;

public:
  ~strong_ref() {
    if (valid()) {
      unref();
    }
  }

  T *get() { return &m_block->value; }

  T &operator*() { return m_block->value; }

  bool valid() { return m_block != nullptr; }

  void invalidate() {
    if (valid()) {
      unref();
      m_block = nullptr;
    }
  }

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

  strong_ref(block_t &block) : m_block(&block) {}

  strong_ref() = default;

  // note: copy is forbidden for simplicity, only create via weak_ref
  // TODO: relax?
  strong_ref(const strong_ref &other) = delete; // TODO: RVO problems

  void unref() {
    --m_block->strong;
    if (m_block->weak == 0) {
      m_block->invoke_deleter(); // TODO: check logic
    }
  }
};

template <typename T> class weak_ref {
  template <typename S, index_t C> friend class weak_cache;

public:
  ~weak_ref() {
    if (valid()) {
      unref();
    }
  }

  weak_ref(const weak_ref &other) : m_block(other.m_block), m_aba(other.m_aba) {
    if (valid()) {
      ref();
    }
  }

  strong_ref<T> get() {
    if (!valid()) {
      return strong_ref<T>();
    }

    // valid
    if (m_block->try_strong_ref()) {
      if (aba_check()) {
        return strong_ref<T>(*m_block);
      }
      m_block->strong_unref();
    }

    invalidate_unchecked();
    return strong_ref<T>();
  }

  void print() {
    if (!valid()) {
      std::cout << "weak nullref" << std::endl;
      return;
    }
    std::cout << "weak ref i " << m_block->index << " s = " << m_block->strong
              << " w = " << m_block->weak << " aba = " << m_block->aba << "("
              << m_aba << ")" << std::endl;
  }

  // todo: operator bool
  bool valid() { return m_block != nullptr; }

  void invalidate() {
    if (valid()) {
      invalidate_unchecked();
    }
  }

  // caution, unchecked and only to be used if this was gotten valid AND locked
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