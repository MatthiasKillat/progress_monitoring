#include "include/monitoring/api.hpp"
#include "monitoring/api.hpp"
#include "monitoring/macros.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <thread>

using namespace std::chrono_literals;

constexpr int A = 1000;
constexpr int B = 10000;
constexpr double MEAN = 5000;
constexpr double STDDEV = 1000;

constexpr double mean(double a, double b) { return (a + b) / 2; }
double stddev(double a, double b) { return (b - a) / (2 * sqrt(3)); }

void work1(int iterations) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::normal_distribution<> dist(MEAN, STDDEV);

  START_THIS_THREAD_MONITORING;

  for (int i = 0; i < iterations; ++i) {
    std::chrono::microseconds t(int64_t(dist(gen)));

    EXPECT_PROGRESS_IN(1000ms, 1);
    std::this_thread::sleep_for(t);
    CONFIRM_PROGRESS;
  }

  STOP_THIS_THREAD_MONITORING;
}

void work2(int iterations) {
  std::random_device rd;
  std::mt19937 gen(rd());

  std::uniform_int_distribution<> dist(A, B);

  START_THIS_THREAD_MONITORING;

  for (int i = 0; i < iterations; ++i) {
    std::chrono::microseconds t(int64_t(dist(gen)));

    EXPECT_PROGRESS_IN(1000ms, 2);
    std::this_thread::sleep_for(t);
    CONFIRM_PROGRESS;
  }

  STOP_THIS_THREAD_MONITORING;
}

int main(void) {

  std::cout << "normal distribution mean " << MEAN << " stddev " << STDDEV
            << std::endl;

  std::cout << "uniform distribution mean " << mean(A, B) << " stddev "
            << stddev(A, B) << std::endl;

  START_ACTIVE_MONITORING(50ms);

  // TODO: currently statistics gathering competes for
  // concurrent shared data access via mutex (change)
  std::thread t1(&work1, 1000);
  std::thread t2(&work1, 1000);
  std::thread t3(&work2, 1000);

  t1.join();
  t2.join();
  t3.join();

  STOP_ACTIVE_MONITORING;

  monitor::print_stats();

  return EXIT_SUCCESS;
}