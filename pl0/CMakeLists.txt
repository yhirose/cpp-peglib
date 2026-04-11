cmake_minimum_required(VERSION 3.14)
project(pl0)

find_package(LLVM REQUIRED CONFIG)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -fexceptions")

add_executable(pl0 pl0.cc)
target_include_directories(pl0 PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(pl0 PRIVATE ..)
target_link_libraries(pl0 LLVM)
