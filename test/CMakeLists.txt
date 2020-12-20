cmake_minimum_required(VERSION 3.1)
project(test)

include_directories(..)

set(CMAKE_CXX_STANDARD 17)

add_executable(test-main test-main.cc test1.cc test2.cc test3.cc)
target_link_libraries(test-main ${add_link_deps})

add_test(
  NAME TestMain
  COMMAND test-main
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
