cmake_minimum_required(VERSION 3.0)
include_directories(..)
add_definitions("-std=c++1y")

add_executable(calc calc.cc)
target_link_libraries(calc pthread)

add_executable(calc2 calc2.cc)
target_link_libraries(calc2 pthread)

add_executable(calc3 calc3.cc)
target_link_libraries(calc3 pthread)

add_executable(calc_readme calc_readme.cc)
target_link_libraries(calc_readme pthread)
