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
    std::atomic<ref_t> strong{1};
    ref_t weak{0};
    count_t aba{0};
    index_t index{0};

    bool try_lock() {
        // c != 0 -> c + 1 
        auto old = strong.load();
        while(old != 0) {
            if(strong.compare_exchange_strong(old, old + 1)) {
                return true;
            }
        }
        return false;
    }

    void unlock() {
        --strong; //assume locked
    }

    bool claim() {
        // c == 0 -> 2
        ref_t exp = 0;
        if(strong.compare_exchange_strong(exp, 2)) {
            return true;
        }
        return false;
    }

    bool reclaim() {
        // c == 1 -> 0
        ref_t exp = 0;
        if(strong.compare_exchange_strong(exp, 2)) {
            return true;
        }
        return false;
    }
};

template<class T, index_t Capacity = 4>
class cache {

    using slot = control_block<T>;
    static constexpr index_t DEFAULT_INDEX = 0;

public:

cache()
{
    for(index_t i=1; i<=Capacity; ++i) {
        auto& s = m_slots[i];
        s.index = i;
        m_unused.push_back(i);
    }
}

slot& get() {
    std::lock_guard<std::mutex> guard(m_mut);
    index_t i;
    // auto count = m_count.fetch_add(1);

    do {
        if(!m_unused.empty()) {
            i = m_unused.front();
            m_unused.pop_front();

            auto& s = get(i);
            if(try_lock(s)) {
                break; // optimize, should not fail
            }
        }

        if(!m_maybeUsed.empty()) {
            i = m_maybeUsed.front();
            m_maybeUsed.pop_front();

            auto& s = get(i);
            if(s.claim()) {
                break;
            }
        }

        // todo: stop after some time if we were unsuccessful

    } while (true);

    // slot is stronly referenced, at least by us
    auto& s = get(i);
    ++s.weak; // we are a weak user

    m_maybeUsed.push_back(i);

    return get(i);
}

void ret(slot& slot) {

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



};
