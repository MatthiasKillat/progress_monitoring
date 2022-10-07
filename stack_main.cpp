#include "stack/api.hpp"

#include <iostream>

using namespace monitor;

void bar() {

  api::tl_push(74);

  api::tl_iterate();
  api::tl_pop();
}

void foo() {

  api::tl_push(73);

  std::cout << "\ncall bar" << std::endl;
  bar();
  std::cout << "end bar\n" << std::endl;
  api::tl_iterate();
  api::tl_pop();
}

int main(void) {
  api::tl_push(72);
  api::tl_stack.print();

  std::cout << "\ncall foo" << std::endl;
  foo();
  std::cout << "end foo\n" << std::endl;

  api::tl_pop();

  return EXIT_SUCCESS;
}