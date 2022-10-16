
#include <future>
#include <iostream>
#include <thread>

#include "monitoring/macros.hpp"
using std::cout;
using std::endl;

using namespace std::chrono_literals;

constexpr auto ADD_TIME_BUDGET = 300ms;
constexpr auto ADD_TIME = ADD_TIME_BUDGET + 1ms;

int add(int a, int b) {
  std::this_thread::sleep_for(ADD_TIME);
  return a + b;
}

int main(int argc, char **argv) {
  START_ACTIVE_MONITORING(100ms);

  // define a deadline handler
  // if we want to external use state we need to capture it
  auto handler = [](monitor::checkpoint &check) {
    (void)check;
    cout << "PANIC !!!" << endl;
  };

  START_THIS_THREAD_MONITORING;

  // install a deadline violation handler
  // can be safely set while there are no deadlines in this thread,
  // could be fused with START_MONITORING but should not be mandatory
  SET_MONITORING_HANDLER(handler);

  std::packaged_task<int(int, int)> task(add);
  auto future = task.get_future();

  EXPECT_PROGRESS_IN(ADD_TIME_BUDGET, 1);

  task(2, 3);
  auto result = future.get();

  CONFIRM_PROGRESS;

  STOP_THIS_THREAD_MONITORING;

  std::cout << "add(2, 3) = " << result << std::endl;

  STOP_ACTIVE_MONITORING;
  return 0;
}
