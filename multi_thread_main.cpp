
#include <future>
#include <iostream>
#include <thread>

// #define MONITORING_OFF
#define MONITORING_ACTIVE
// #define MONITORING_PASSIVE

#include "monitoring_interface.hpp"
using std::cout;
using std::endl;

using namespace std::chrono_literals;

auto constexpr LOCK_BUDGET = 50ms;
auto constexpr LOCK_TIME = LOCK_BUDGET - 1ms;

constexpr auto BUDGET1 = 200ms;
constexpr auto SLEEP1 = 300ms;

constexpr auto BUDGET2 = 200ms;
constexpr auto SLEEP2 = 210ms;

// this could be called on a real mutex or similar lockable resource
void lock() { std::this_thread::sleep_for(LOCK_TIME); }
void unlock() {}

// function is called by mutiple threads, hence budgets need to be passed at runtime
void someAlgorithm(int id, std::chrono::milliseconds budget,
                   std::chrono::milliseconds sleep) {

  static std::atomic<int> s_id{1};
  auto handler = [=]() { cout << "Thread " << id << " PANIC !!!" << endl; };

  START_MONITORING;

  EXPECT_PROGRESS_IN(LOCK_BUDGET);
  lock(); // lock mock
  CONFIRM_PROGRESS;

  // OK to set, there is no active deadline yet 
  // (execution of previous - if any - handler will finish before)
  SET_DEADLINE_HANDLER(handler);

  EXPECT_PROGRESS_IN(budget);
  std::this_thread::sleep_for(sleep);
  CONFIRM_PROGRESS;

  unlock();

  STOP_MONITORING;
}

int main(int argc, char **argv) {
  ACTIVATE_MONITORING(100ms);

  std::thread t1(someAlgorithm, 1, BUDGET1, SLEEP1);
  std::thread t2(someAlgorithm, 2, BUDGET2, SLEEP2);

  t1.join();
  t2.join();

  DEACTIVATE_MONITORING;
  return 0;
}
