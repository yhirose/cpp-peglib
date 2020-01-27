cmake_minimum_required(VERSION 3.1)
project(test)

enable_language(CXX)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(..)
add_executable(test-main test.cc)
target_link_libraries(test-main ${add_link_deps})

add_test(TestMain test-main)
