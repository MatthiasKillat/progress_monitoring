cmake_minimum_required(VERSION 3.10)
project(thread-monitoring LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

include_directories(include)

add_executable(monitoring
  main.cpp src/monitoring.cpp
)

target_link_libraries(monitoring PRIVATE Threads::Threads)

enable_language(CUDA)

add_executable(cuda_monitoring
  cuda_main.cpp cuda_computation.cu src/monitoring.cpp
)

set_property(TARGET cuda_monitoring PROPERTY CUDA_SEPARABLE_COMPILATION ON)


