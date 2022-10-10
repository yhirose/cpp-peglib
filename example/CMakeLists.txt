cmake_minimum_required(VERSION 3.14)
project(example)

add_executable(calc calc.cc)
target_link_libraries(calc CppPegLib)

add_executable(calc2 calc2.cc)
target_link_libraries(calc2 CppPegLib)

add_executable(calc3 calc3.cc)
target_link_libraries(calc3 CppPegLib)

add_executable(calc4 calc4.cc)
target_link_libraries(calc4 CppPegLib)

add_executable(calc5 calc5.cc)
target_link_libraries(calc5 CppPegLib)

add_executable(indent indent.cc)
target_link_libraries(indent CppPegLib)
