#include <iostream>

#include "include/cache.hpp"

int main(void) {

   using MyCache = weak_cache<int, 1>;

   MyCache cache;

  auto weak = cache.get_weak_ref();
  weak.print();

  {
    auto strong = weak.get();
    strong.print();

    weak_ref<int> weak2(weak);
    weak.print();
    {
      weak_ref<int> weak3(weak);
      weak.print();
    }
    weak.print();
  }
  weak.print();

  auto new_weak = cache.get_weak_ref();
  new_weak.print();

  auto strong = weak.get();

  std::cout << "***********" << std::endl;
  new_weak.print(); // only one user (invalidated 1)
  strong.print(); // nullref
  new_weak.print();

  auto strong1 = new_weak.get();
  strong1.print();

  new_weak.invalidate();
  strong1.print();

  //strong1.invalidate();
  strong1.print();

  auto new_weak2 = cache.get_weak_ref();
  new_weak2.print();


  return EXIT_SUCCESS;
}