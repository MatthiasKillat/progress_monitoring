#include <unistd.h>

pid_t bar(int n) {
  if (n > 0)
    return bar(n - 1);
  return getpid();
}

pid_t foo() { return bar(1); }