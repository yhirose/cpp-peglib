cmake_minimum_required(VERSION 3.1)
project(peglint)

include_directories(..)

set(CMAKE_CXX_STANDARD 17)

add_executable(peglint peglint.cc)
target_link_libraries(peglint ${add_link_deps})
