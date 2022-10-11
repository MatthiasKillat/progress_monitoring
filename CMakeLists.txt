cmake_minimum_required(VERSION 3.14)
project(thread-monitoring LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_language(CUDA)

find_package(Threads REQUIRED)

include_directories(include)

add_executable(single_monitoring
  single_thread_main.cpp src/monitoring.cpp
)

target_link_libraries(single_monitoring PRIVATE Threads::Threads)

add_executable(multi_monitoring
  multi_thread_main.cpp src/monitoring.cpp
)

target_link_libraries(multi_monitoring PRIVATE Threads::Threads)

add_executable(async_monitoring
  async_main.cpp src/monitoring.cpp
)

target_link_libraries(async_monitoring PRIVATE Threads::Threads)

add_executable(cuda_monitoring
  cuda_main.cpp cuda_computation.cu src/monitoring.cpp
)

set_property(TARGET cuda_monitoring PROPERTY CUDA_SEPARABLE_COMPILATION ON)

add_executable(stack
  stack_main.cpp
)

add_executable(weak_cache
  cache_main.cpp
)

add_executable(monitoring
  monitoring_main.cpp
)

target_link_libraries(monitoring PRIVATE Threads::Threads)


## Tests

enable_testing()

add_executable(
  test_monitoring
  ./test/monitoring.cpp
)
target_link_libraries(
  test_monitoring
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_monitoring)


