cmake_minimum_required(VERSION 3.10)
project(thread-monitoring)

set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

include_directories(include)

add_executable(monitoring
  main.cpp src/monitoring.cpp
)

target_link_libraries(monitoring PRIVATE Threads::Threads)


