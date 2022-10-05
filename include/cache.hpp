#pragma once

#include <atomic>
#include <mutex>
#include <deque>
#include <array>
#include <functional>

#include <iostream>
#include <type_traits>

using ref_t = uint64_t;
using count_t = uint64_t;
using index_t = uint32_t;

// TODO: refactor, improve efficiency, check correctness of intrictae details
// explore use case

// detail, will not be exposed
template<typename T>
struct control_block {
    std::aligned_storage_t<sizeof(T), alignof(T)> storage;
    std::atomic<ref_t> strong{1};
    std::atomic<ref_t> weak{0};
    std::atomic<count_t> aba{0};
    std::atomic<index_t> index{0};

    // note that the deleter is shared across all blocks from the same cache
    std::function<void(control_block<T>&)>* deleter{nullptr}; // needed to e.g. return block to unused cache
   
    // strong = 0 : unused
    // strong = 1 : cache exclusive ownership, no change of strong from any ref
    // strong = 2 : unused if weak = 0

    bool try_strong_ref() {
        // c > 1 -> c + 1
        auto old = strong.load();
        while(old > 1) {
            if(strong.compare_exchange_strong(old, old + 1)) {
                return true;
            }
        }
        return false;
    }

    void strong_unref() {
        --strong; //assume locked before
    }

    // TODO: prove correctness and optimize
    bool claim() {
        // c == 0 -> 1
        ref_t exp = 0;
        if(strong.compare_exchange_strong(exp, 1)) {
            return true;
        }
        // c == 2 -> 1
        exp = 2; // only weak refs
        if(strong.compare_exchange_strong(exp, 1)) {
            return true;
        }
        return false;
    }

    bool reclaim() {
        // c == 2 -> 0
        ref_t exp = 2;
        if(strong.compare_exchange_strong(exp, 0)) {
            return true;
        }
        return false;
    }

    void invoke_deleter() {
        (*deleter)(*this);
    }
};

template<class T, index_t Capacity>
class cache {
    static constexpr index_t DEFAULT_INDEX = 0;
    
    using block = control_block<T>;

public:

cache()
{    
    m_deleter = [this](block& b) {
        std::cout << "deleter called" << std::endl;
        this->ret(b);
    };

    for(index_t i=0; i<Capacity; ++i) {
        auto& s = m_slots[i];
        s.index = i;
        s.deleter = &m_deleter;
        m_unused.push_back(i);
    }
}

block* get() {
    std::lock_guard<std::mutex> guard(m_mut); // TODO: no mutex later
    index_t i;
    
    do {
        if(!m_unused.empty()) {
            i = m_unused.front();
            m_unused.pop_front();

            auto& s = get(i);
            claim_uncontended(s);
            m_maybeUsed.push_back(i);
            return &s;
        }

        auto n = m_maybeUsed.size();

        for(uint32_t i = 0; i<n; ++i) {
            if(!m_maybeUsed.empty()) {
                i = m_maybeUsed.front();
                m_maybeUsed.pop_front();

                auto& s = get(i);
                if(claim_contended(s)) { // may fail if used
                    m_maybeUsed.push_back(i);
                    return &s;
                } else {
                    // it is now at the end of queue again
                    m_maybeUsed.push_back(i);
                }
            }
        }
        // todo: stop after some time if we were unsuccessful
        break;
    } while (true);
  
    return nullptr;
}

void ret(block& block) {

     if(block.weak == 0) {
        if(block.reclaim()) {
            // there can be no strong refs
            if(block.weak == 0) { // required check?
                std::lock_guard<std::mutex> guard(m_mut);
                std::cout << "moved to unused " << block.index << std::endl;
                m_unused.push_back(block.index);
            } else {
               // still in maybeUsed
            }
        }
     }

}

private:

std::mutex m_mut;
// 0 is a special block
std::array<block, Capacity + 1> m_slots;
std::deque<index_t> m_maybeUsed;
std::deque<index_t> m_unused; // not needed to be a deque later
std::atomic<count_t> m_count{73};

std::function<void(block&)> m_deleter;

block& get(index_t i) {
    return m_slots[i];
}

count_t update_count() {
    return m_count.fetch_add(1);
}

void claim_uncontended(block& s) {
    //can assume weak == 0 == strong
    s.strong.store(1);
    s.aba = update_count();
}

bool claim_contended(block& s) {
    if(!s.claim()) {
        return false;
    }
    // now we know strong = 1, weak = ?
    // no strong refs can be created by others (requires strong >= 2)

    s.aba = update_count(); // will invalidate old weak refs
    return true;
}

};

// if the block is used, cache holds one ref
// cache gives a weak ref 
// weak refs can generate strong refs
// weak refs can generate weak refs
// strong refs can only generate strong refs

// note weak_ref and strong_ref are not thread_safe
// TODO: move refs to separate file
template<class T, index_t Capacity>
class weak_cache;

template<class T>
class weak_ref;

template<typename T>
class strong_ref {
    friend class weak_ref<T>;
    using block_t = control_block<T>;

public:
    ~strong_ref() {
        if(valid()) {
            unref();
        }
    }

    T* get() {
        return &m_block->value;
    }

    T& operator*() {
        return m_block->value;
    }

    bool valid() {
        return m_block != nullptr;
    }

    void invalidate() {
        if(valid()) {
            unref();
            m_block = nullptr;
        }
    }

    void print() {
        if(!valid()) {
            std::cout << "strong nullref" << std::endl;
            return;
        }

        std::cout << "strong ref i " << m_block->index << " s = " << m_block->strong << " w = " << m_block->weak 
        << " aba = " << m_block->aba << std::endl;
    }

private:
    block_t* m_block{nullptr};

    strong_ref(block_t& block) : m_block(&block) {
    }

    strong_ref() = default;

    // note: copy is forbidden for simplicity, only create via weak_ref
    // TODO: relax?
    strong_ref(const strong_ref & other) = delete; // TODO: RVO problems

    void unref() {
        --m_block->strong;
        // TODO: free it, need deleter in block (or static, if there is only one cache)
        if(m_block->weak == 0) {
            m_block->invoke_deleter(); // TODO: check logic
        }
    }
};

template<typename T>
class weak_ref {
    template<typename S, index_t C>
    friend class weak_cache;

public: 
    ~weak_ref() {
        if(valid()) {
            unref();
        }
    }

    weak_ref(const weak_ref &other) : m_block(other.m_block), m_aba(other.m_aba) {
        if(valid()) {
            ref();
        }
    }

    strong_ref<T> get() {
        if(!valid()) {
            return strong_ref<T>();
        }

        // valid
        if(m_block->try_strong_ref()) {
            if(aba_check()) {
                return strong_ref<T>(*m_block);
            }
            m_block->strong_unref();
        }

        invalidate_unchecked();
        return strong_ref<T>();
    }

    void print() {
        if(!valid()) {
            std::cout << "weak nullref" << std::endl;
            return;
        }
        std::cout << "weak ref i " << m_block->index << " s = " << m_block->strong << " w = " << m_block->weak
        << " aba = " << m_block->aba << "(" << m_aba << ")" << std::endl;
    }
    
    // todo: operator bool
    bool valid() {
        return m_block != nullptr;
    }

    void invalidate() {
        if(valid()) {
            invalidate_unchecked();
        }
    }

    // caution, unchecked and only to be used if this was gotten valid AND locked
    void unlock() {
        m_block->strong_unref();
    }

private:
    control_block<T>* m_block{nullptr};
    count_t m_aba;

    weak_ref(control_block<T>& block) : m_block(&block) {
        ref();
        m_aba = m_block->aba;
    }

    weak_ref() = default;

    bool aba_check() {
        // better/more powerful sanity check needed/easily possible?
        return m_aba == m_block->aba.load();
    }

    void ref() {
        ++m_block->weak;
    }

    void unref() {
        if(--m_block->weak == 0) {
            m_block->invoke_deleter();
        }
    }

    void invalidate_unchecked() {
        unref();
        m_block = nullptr;
    }
};

template<class T, index_t Capacity = 4>
class weak_cache {

    using block = control_block<T>;

public:

    // can copy returned ref and obtain a strong one
    weak_ref<T> get_weak_ref() {
        auto block = m_cache.get();
        if(block) {
             // block.strong is 1 and only this get can change it
             // (if stalled, the block is lost)
             block->strong.store(2); // now others may try to generate strong refs again but will fail due to aba
             return weak_ref<T>(*block);
        }
        return weak_ref<T>();
    }

    weak_ref<T> get_locked_ref() {
        auto block = m_cache.get();
        if(block) {
            // we have an additional striong ref count
            block->strong.store(3); // now others may try to generate strong refs again but will fail due to aba 
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