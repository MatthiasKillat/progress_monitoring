#include <iostream>

#include "cuda_computation.h"

constexpr size_t N = 1000;

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

int main(void) {
  init(1);
  print();

  auto success = cuda_computation(buffer, N);

  if (!success) {
    std::terminate();
  }

  print();
  return 0;
}