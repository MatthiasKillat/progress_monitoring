#include "include/monitoring/api.hpp"
#include "monitoring/api.hpp"
#include "monitoring/macros.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

using namespace std::chrono_literals;

void work(int iterations) {
  std::random_device rd;
  std::mt19937 gen(rd());

  // high variance
  std::uniform_int_distribution<> dist(1000, 10000);

  // low variance
  // std::uniform_int_distribution<> dist(9999, 10000);

  START_THIS_THREAD_MONITORING;

  for (int i = 0; i < iterations; ++i) {
    std::chrono::microseconds t(dist(gen));

    EXPECT_PROGRESS_IN(1000ms, 1);
    std::this_thread::sleep_for(t);
    CONFIRM_PROGRESS;
  }

  STOP_THIS_THREAD_MONITORING;
}

int main(void) {

  START_ACTIVE_MONITORING(50ms);

  std::thread t(&work, 1000);

  t.join();

  STOP_ACTIVE_MONITORING;

  monitor::print_stats();

  return EXIT_SUCCESS;
}