cmake_minimum_required(VERSION 3.0)
include_directories(.)
add_definitions("-std=c++1y")

add_executable(peglint lint/cmdline/peglint.cc)
target_link_libraries(peglint pthread)

add_executable(playground lint/online/main.cc)
target_link_libraries(playground pthread)

add_executable(test-main test/test.cc)
target_link_libraries(test-main pthread)

add_executable(calc example/calc.cc)
target_link_libraries(calc pthread)

add_executable(calc2 example/calc2.cc)
target_link_libraries(calc2 pthread)

add_executable(calc3 example/calc3.cc)
target_link_libraries(calc3 pthread)

add_executable(pl0 language/pl0/pl0.cc)
target_link_libraries(pl0 pthread)

add_executable(culebra language/culebra/main.cc)
target_link_libraries(culebra pthread)
