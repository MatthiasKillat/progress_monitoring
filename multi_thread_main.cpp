
#include <future>
#include <iostream>
#include <thread>

// #define MONITORING_OFF
#define MONITORING_ACTIVE
// #define MONITORING_PASSIVE

#include "monitoring_interface.hpp"
using namespace monitor;
using std::cout;
using std::endl;

using namespace std::chrono_literals;

constexpr auto ADD_TIME = 300ms;
constexpr auto EXPECTED_ADD_TIME = ADD_TIME - 1ms;

int add(int a, int b) {
  std::this_thread::sleep_for(ADD_TIME);
  return a + b;
}

auto constexpr LOCK_TIME = 50ms;
auto constexpr LOCK_DEADLINE = LOCK_TIME - 1ms;

// this could be called on a real mutex or similar resource
void lock() { std::this_thread::sleep_for(LOCK_TIME); }
void unlock() {}

void work(std::chrono::milliseconds deadline, std::chrono::milliseconds sleep) {

  static std::atomic<int> s_id{1};
  auto id = s_id.fetch_add(1);
  auto handler = [=]() { cout << "Thread " << id << " PANIC !!!" << endl; };

  START_MONITORING
  // can be safely set since there are no deadlines
  // specific to this thread yet,
  // could be fused with START_MONITORING but should not be mandatory
  SET_DEADLINE_HANDLER(handler);

  EXPECT_PROGRESS_IN(LOCK_DEADLINE);
  lock();
  CONFIRM_PROGRESS;

  // todo: future/promise demo

  // note: only one deadline at a time (multiple would require a stack/prio
  // list to keep track of them)

  EXPECT_PROGRESS_IN(deadline);
  std::this_thread::sleep_for(sleep);
  CONFIRM_PROGRESS;

  unlock();

  STOP_MONITORING;
}

constexpr auto S1 = 1100ms;
constexpr auto E1 = 1000ms;

constexpr auto S2 = 1050ms;
constexpr auto E2 = 1010ms;

int main(int argc, char **argv) {
  ACTIVATE_MONITORING(100ms); // low frequency to save cycles

  std::thread t1(work, E1, S1);
  std::thread t2(work, E2, S2);

  t1.join();
  t2.join();

  std::packaged_task<int(int, int)> task(add);
  auto future = task.get_future();

  // could always start monitoring thread if not yet started (requires check
  // though ...)
  START_MONITORING;
  EXPECT_PROGRESS_IN(EXPECTED_ADD_TIME);
  // this could also be some async GPU computation
  task(2, 3);
  auto result = future.get();
  CONFIRM_PROGRESS;
  STOP_MONITORING;

  std::cout << "add(2, 3) = " << result << std::endl;

  DEACTIVATE_MONITORING;
  return 0;
}

// specialize for ms (templated thread wrapper)
// chrono literals support
// active monitoring only mode

// unique location codes instead of source location
