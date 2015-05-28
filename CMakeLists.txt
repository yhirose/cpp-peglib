cmake_minimum_required(VERSION 3.0)
include_directories(.)
add_definitions("-std=c++1y")

add_executable(peglint lint/peglint.cc)
add_executable(test-main test/test.cc)

#add_executable(calc example/calc.cc)
#add_executable(calc2 example/calc2.cc)
#add_executable(calc3 example/calc3.cc)

