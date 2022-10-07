#include "include/monitoring/api.hpp"
#include "monitoring/api.hpp"
#include "monitoring/thread_monitor.hpp"
#include "monitoring/thread_state.hpp"

#include <iostream>

// using namespace monitor;

int main(void) {
  //   thread_monitor tmon;

  //   auto t1 = tmon.register_this_thread();
  //   auto t2 = tmon.register_this_thread();
  //   auto t3 = tmon.register_this_thread();

  //   std::cout << t1 << std::endl;
  //   std::cout << t2 << std::endl;
  //   std::cout << t3 << std::endl;

  //   tmon.deregister(*t1);
  //   t3 = tmon.register_this_thread();
  //   std::cout << t3 << std::endl;

monitor::start_monitoring();

monitor::stop_monitoring();

return EXIT_SUCCESS;
}