#include <iostream>

#include "include/cache.hpp"

int main(void) {

   using MyCache = weak_cache<int, 4>;

   MyCache cache;

  auto weak = cache.get_weak_ref();
  auto strong = weak.get();

  return EXIT_SUCCESS;
}