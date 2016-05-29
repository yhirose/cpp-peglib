cmake_minimum_required(VERSION 2.8)
include_directories(..)
add_definitions("-std=c++1y")

add_executable(test-main test.cc)
target_link_libraries(test-main pthread)
