#include <iostream>

#include "include/stack.hpp"
#include "stack.hpp"

thread_local Stack stack;

void bar() {

  StackGuard g(stack, 21);
  std::cout << "end bar" << std::endl;

  stack.concurrent_iterate();
}

void foo() {

  StackGuard g(stack, 73);
  bar();
  std::cout << "end foo" << std::endl;
  stack.concurrent_iterate();
}

int main(void) {
  stack.print();

  StackGuard g(stack, 42);
  foo();
  stack.concurrent_iterate();

  Cache<4, 2> cache;

  cache.add(66);
  if (cache.find(66)) {
    std::cout << "66 in cache\n";
  }

  if (cache.find(70)) {
    std::cout << "70 in cache\n";
  }

  return EXIT_SUCCESS;
}