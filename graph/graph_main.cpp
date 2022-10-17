#include <iostream>
#include <mutex>

#include <unistd.h>

// std::mutex g_mutex;
namespace hap {
namespace ablab {
class Baz {
public:
  Baz() : x{73} {}
  void baz() { std::cout << x << std::endl; }

  int x;
};
} // namespace ablab

pid_t bar(int n) {
  if (n > 0)
    return bar(n - 1);
  return getpid();
}

pid_t *foo() {
  // std::lock_guard<std::mutex> g(g_mutex);
  return new pid_t(bar(1));
}

} // namespace hap

using namespace hap;

int main(void) {

  pid_t *r = foo();
  // std::cout << *r << std::endl;

  delete r;

  ablab::Baz baz;
  baz.baz(); // noop but detected as call

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
