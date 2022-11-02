cmake_minimum_required(VERSION 3.14)
project(thread-monitoring LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

#set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

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

add_executable(async_monitoring
  async_main.cpp
)

target_link_libraries(async_monitoring PRIVATE Threads::Threads)

add_executable(cuda_monitoring
  cuda_main.cpp cuda_computation.cu
)

set_property(TARGET cuda_monitoring PROPERTY CUDA_SEPARABLE_COMPILATION ON)

add_executable(weak_cache
  cache_main.cpp
)

add_executable(monitoring
  monitoring_main.cpp
)

target_link_libraries(monitoring PRIVATE Threads::Threads)

add_executable(statistics
  statistics_main.cpp
)

target_link_libraries(statistics PRIVATE Threads::Threads)

add_executable(graph
  ./graph/graph_main.cpp
)

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

## Benchmark

find_package(benchmark REQUIRED)

add_executable(
  benchmark_monitoring
  ./benchmark/monitoring_benchmark.cpp
)
target_link_libraries(
  benchmark_monitoring
  benchmark::benchmark
)

add_executable(
  benchmark_general
  ./benchmark/general_benchmark.cpp
)
target_link_libraries(
  benchmark_general
  benchmark::benchmark
)


