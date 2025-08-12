cmake_minimum_required(VERSION 3.31)
project(yadro-cpp-course)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(foonathan_memory 0.7 REQUIRED)

add_compile_options(-std=c++23 -Wall -Wextra  -g -Og)
add_link_options(-rdynamic)
add_executable(main src/main.cpp src/coro.cpp)
