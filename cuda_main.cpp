#include <iostream>

#include "cuda_computation.h"

// #define MONITORING_OFF
#define MONITORING_ACTIVE
// #define MONITORING_PASSIVE

#include "monitoring_interface.hpp"

using namespace std::chrono_literals;

constexpr size_t N = 1000;
// constexpr size_t N = 10000000; // vary to provoke deadline violations
constexpr auto TIME_BUDGET = 1ms;
constexpr size_t ITERATIONS = 20;

float buffer[N];

void init(float x) {
  for (auto &v : buffer) {
    v = x;
  }
}

void print() {
  std::cout << buffer[0] << " " << buffer[N / 2] << " " << buffer[N - 1]
            << std::endl;
}

bool dummy_computation(float *host, size_t n) {
  std::this_thread::sleep_for(2ms);
  host[0] += 1;
  host[N / 2] += 1;
  host[N - 1] += 1;
  return true;
}

int main(void) {
  init(0);
  print();

  // this can be automated with further abstraction
  // 1) bundle with init function
  // 2) specialized monitored_threads - call wrapper, ctor, dtor

  ACTIVATE_MONITORING(100ms); // call once, global
  START_MONITORING;           // call per monitored thread

  for (int i = 0; i < ITERATIONS; ++i) {
    EXPECT_PROGRESS_IN(1ms); // call per monitored section

    // first run takes longer, likely due to some GPU init
    auto success = cuda_computation1(buffer, N);
    // auto success = dummy_computation(buffer, N);
    CONFIRM_PROGRESS; // corresponds to previous EXPECT_PROGRESS_IN

    if (!success) {
      std::cout << "failure - break" << std::endl;
      break;
    }
    print();
  }

  STOP_MONITORING;       // corresponds to START_MONITORING
  DEACTIVATE_MONITORING; // corresponds to ACTIVATE_MONITORING

  return EXIT_SUCCESS;
}