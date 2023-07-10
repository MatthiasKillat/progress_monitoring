#include <sys/shm.h>
#include <unistd.h>

#include <iostream>
#include <thread>

#include "state/single_wait.hpp"

constexpr size_t SIZE = sizeof(WaitState) + alignof(WaitState);

using namespace std;

void react(int signal) {
  if (signal & 1) {
    cout << "bit 1" << endl;
  }

  if (signal & 2) {
    cout << "bit 2" << endl;
  }

  if (signal & 4) {
    cout << "bit 3" << endl;
  }
  if (signal & 8) {
    cout << "bit 4" << endl;
  }
}

int main(int argc, char **argv) {

  int shm_id = shmget(IPC_PRIVATE, SIZE, IPC_CREAT | 0666);
  if (shm_id < 0) {
    exit(1);
  }

  auto *shm_ptr = shmat(shm_id, NULL, 0);

  cout << "Shared memory created " << shm_ptr << endl;

  auto state_ptr = new (shm_ptr) WaitState();

  cout << "State constructed " << state_ptr << endl;

  int status = fork();
  if (status < 0) {
    cout << "Fork failed" << endl;
    exit(1);
  }

  if (status == 0) {
    cout << "Child " << getpid() << endl;
    Notifier notifier(*state_ptr);

    int signal1 = 3;
    std::cout << "Child signals " << signal1 << std::endl;
    notifier.notify(signal1);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    int signal2 = 5;
    std::cout << "Child signals " << signal2 << std::endl;
    notifier.notify(signal2);

    std::this_thread::sleep_for(std::chrono::seconds(4));

    int signal3 = 8;
    std::cout << "Child signals " << signal3 << std::endl;
    notifier.notify(signal3);

    while (true) {
      // prevent the parent from getting stuck due to timing
      notifier.notify();
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

  } else {
    cout << "Parent " << getpid() << endl;

    SingleWait waitable(*state_ptr);
    std::this_thread::sleep_for(std::chrono::seconds(3));

    auto signal = waitable.wait();
    std::cout << "Parent received signal " << signal << " count "
              << waitable.count() << endl;
    react(signal);

    signal = waitable.wait();
    std::cout << "Parent received signal " << signal << " count "
              << waitable.count() << endl;
    react(signal);
  }

  exit(0);
}