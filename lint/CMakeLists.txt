cmake_minimum_required(VERSION 3.14)
project(peglint)

add_executable(peglint peglint.cc)

target_link_libraries(peglint CppPegLib)
