cmake_minimum_required(VERSION 2.8)
include_directories(..)
add_compile_options(${cxx11_options} ${warning_options})

add_executable(test-main test.cc)
target_link_libraries(test-main ${add_link_deps})

add_test(TestMain test-main)
