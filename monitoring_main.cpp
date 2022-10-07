#include "include/monitoring/api.hpp"
#include "include/source_location.hpp"
#include "include/stack/entry.hpp"
#include "monitoring/api.hpp"
#include "monitoring/thread_monitor.hpp"
#include "monitoring/thread_state.hpp"

#include <chrono>
#include <iostream>
#include <thread>

// using namespace monitor;
using namespace std::chrono_literals;

void handler(monitor::checkpoint &check) {
  (void)check;
  std::cout << "HANDLER CALLED" << std::endl;
}

void work1() {
  monitor::start_this_thread_monitoring();
  monitor::set_this_thread_handler(handler);

  monitor::expect_progress_in(10ms, 42, THIS_SOURCE_LOCATION);
  monitor::expect_progress_in(1000ms, 73, THIS_SOURCE_LOCATION);
  monitor::expect_progress_in(100ms, 21, THIS_SOURCE_LOCATION);

  std::this_thread::sleep_for(2000ms);

  monitor::confirm_progress(THIS_SOURCE_LOCATION);
  monitor::confirm_progress(THIS_SOURCE_LOCATION);
  monitor::confirm_progress(THIS_SOURCE_LOCATION);

  monitor::stop_this_thread_monitoring();
}

void work2() {
  monitor::start_this_thread_monitoring();

  monitor::expect_progress_in(500ms, 66, THIS_SOURCE_LOCATION);

  std::this_thread::sleep_for(3000ms);

  monitor::confirm_progress(THIS_SOURCE_LOCATION);

  monitor::stop_this_thread_monitoring();
}

int main(void) {

  std::thread t1(&work1);
  std::thread t2(&work2);
  t1.join();
  t2.join();

  return EXIT_SUCCESS;
}