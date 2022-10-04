#pragma once

#include <atomic>
#include <mutex>
#include <deque>
#include <array>

using ref_t = uint64_t;
using count_t = uint64_t;
using index_t = uint32_t;

template<typename T>
struct control_block{
    T value; // TODO: placement new type
    std::atomic<ref_t> strong{1}; // 0 unclaimed, 1 claimed by cache, exclusive, >=2 can be locked
    std::atomic<ref_t> weak{0};
    std::atomic<count_t> aba{0};
    std::atomic<index_t> index{0};

    // rules
    // weak = 0 -> strong = 0
    // strong = 1 : cache exclusive ownership, cannot be changed via cache

    bool try_lock() {
        // c > 1 -> c + 1 
        auto old = strong.load();
        while(old > 1) {
            if(strong.compare_exchange_strong(old, old + 1)) {
                return true;
            }
        }
        return false;
    }

    void unlock() {
        --strong; //assume locked before
    }

    bool claim() {
        // c == 0 -> 1
        ref_t exp = 0;
        if(strong.compare_exchange_strong(exp, 1)) {
            return true;
        }
        return false;
    }

    bool reclaim() {
        // c == 1 -> 0
        ref_t exp = 0;
        if(strong.compare_exchange_strong(exp, 0)) {
            return true;
        }
        return false;
    }
};




template<class T, index_t Capacity>
class cache {
    static constexpr index_t DEFAULT_INDEX = 0;

    // todo rename (block)
    using slot = control_block<T>;

public:

cache()
{
    for(index_t i=1; i<=Capacity; ++i) {
        auto& s = m_slots[i];
        s.index = i;
        m_unused.push_back(i);
    }
}

slot& get_or(slot& alt) {
    std::lock_guard<std::mutex> guard(m_mut); // TODO: no mutex later
    index_t i;
    
    do {
        if(!m_unused.empty()) {
            i = m_unused.front();
            m_unused.pop_front();

            auto& s = get(i);
            claim_uncontended(s);
            m_maybeUsed.push_back(i); // could have multiples now
            return s;
        }

        auto n = m_maybeUsed.size();

        for(uint32_t i = 0; i<n; ++i) {
            if(!m_maybeUsed.empty()) {
                i = m_maybeUsed.front();
                m_maybeUsed.pop_front();

                auto& s = get(i);
                if(claim_contended(s)) { // may fail if used
                    m_maybeUsed.push_back(i);
                    return s;
                } else {
                    // it is now at the end of queue again
                    m_maybeUsed.push_back(i);
                }
            }
        }
        // todo: stop after some time if we were unsuccessful
        break;
    } while (true);
  
    return alt;
}

void ret(slot& slot) {

     if(slot.weak == 0) {
        if(slot.reclaim()) {
            // there can be no strong refs
            if(slot.weak == 0) {
                std::lock_guard<std::mutex> guard(m_mut);
                m_unused.push_back(slot.index);
            } else {
               // still in maybeUsed
            }
        }
     }     

}

private:

std::mutex m_mut;
// 0 is a special slot
std::array<slot, Capacity + 1> m_slots;
std::deque<index_t> m_maybeUsed;
std::deque<index_t> m_unused; // not needed to be a deque later
std::atomic<count_t> m_count{0};

slot& get(index_t i) {
    return m_slots[i];
}

count_t update_count() {
    return m_count.fetch_add(1);
}

void claim_uncontended(slot& s) {
    //can assume weak == 0 == strong
    s.weak.store(1);
    s.strong.store(1);
    s.aba = update_count();
    ++s.strong; // could set to 2 directly?
}

bool claim_contended(slot& s) {
    if(!s.claim()) {
        return false; // there was a strong ref
    }
    // now we know strong = 1, weak = ?
    // no strong refs can be created by others (requires strong >= 2)

    s.aba = update_count(); // will invalidate old weak refs
    s.weak = 1; // we declare we are the only weak ref now

    ++s.strong; // now others may try to generate strong refs again but will fail due to aba
    return true;
}

};

// from unused queue: we know there are no weak refs, can claim uncontended
//
// from maybe used queue
// if strong > 0 -> skip
// if no strong ref (strong = 0)
//    if no weak ones -> unused, free to use
// so weak > 0, strong = 0
//    CAS strong 0 -> 1
//    if fail, skip
//    if succeed we have now at least 1 strong ref to it
//    we know there were weak refs in the past
//    change aba counter
//    if strong is still 1, no one has locked it (and we can set weak refs to 1?)
//    otherwise skip it


// if the slot is used, cache holds one ref
// cache gives a weak ref 
// weak refs can generate strong refs
// weak refs can generate weak refs
// strong refs can only generate strong refs

// TODO: move refs to separate file
template<class T, index_t Capacity>
class weak_cache;

template<class T>
class weak_ref;

template<typename T>
class strong_ref {
    friend class weak_ref<T>;
    // TODO: ref counting
public:

private:
    control_block<T>* m_block;

    strong_ref(control_block<T>& block) : m_block(&block) {
    }
};

template<typename T>
class weak_ref {
    template<typename S, index_t C>
    friend class weak_cache;

public: 
    // TODO: ref counting

    strong_ref<T> get() {
        return strong_ref<T>(*m_block);
    }

private:
    control_block<T>* m_block;
    count_t m_aba;

    weak_ref(control_block<T>& block) : m_block(&block) {
        m_aba = m_block->aba;
    }

    bool validate() {
        return m_aba = m_block->aba.load();

    }
};



template<class T, index_t Capacity = 4>
class weak_cache {

    using block = control_block<T>;

public:

    weak_ref<T> get_weak_ref() {
        auto& block = m_cache.get_or(m_defaultBlock); // todo: change API to pointer
        return weak_ref<T>(block);
    }

    // strong_ref<T> get_strong_ref() {
    //     return strong_ref<T>();
    // }

    // returning is not required, will happen automatically

private:
    block m_defaultBlock;
    cache<T, Capacity> m_cache;
};