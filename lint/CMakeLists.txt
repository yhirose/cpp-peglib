cmake_minimum_required(VERSION 3.14)
project(peglint)

include_directories(..)

add_executable(peglint peglint.cc)
target_link_libraries(peglint ${add_link_deps})
