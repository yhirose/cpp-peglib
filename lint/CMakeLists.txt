cmake_minimum_required(VERSION 3.14)
project(peglint)

add_executable(peglint peglint.cc)
target_compile_features(peglint PRIVATE cxx_std_17)
target_include_directories(peglint PRIVATE ..)
target_link_libraries(peglint ${add_link_deps})
