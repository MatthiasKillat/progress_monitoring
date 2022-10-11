#include <iostream>
#include <thread>

#include <unistd.h>

pid_t bar() { return getpid(); }

pid_t *foo() { return new pid_t(bar()); }

int main(void) {

  pid_t *r = foo();
  std::cout << *r << std::endl;

  return EXIT_SUCCESS;
}

// my_callgraph file output can be read or ananlyzed by script
// use clang++ -S -emit-llvm graph_main.cpp -o - | opt --print-callgraph 2>
// my_callgraph
//
// OR
//
// clang++ -S -emit-llvm graph_main.cpp -o outfile
// opt --dot-callgraph outfile
// visualize the callgraph (more precisely usually a forest)
// dot -Tpng -ocallgraph.png callgraph.dot
//
// we can demangle the names in the graph
// llvm-cxxfilt < outfile.callgraph.dot > demangled_callgraph.dot
// but this graph cannot be printed by dot (but analyzed via script to find
// calls and their ancestors)
