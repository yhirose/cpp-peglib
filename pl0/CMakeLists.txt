cmake_minimum_required(VERSION 2.8)
include_directories(..)

if(MSVC)
  add_compile_options(${cxx11_options} /W3)
else()
  add_compile_options(${cxx11_options})
endif()

find_package(LLVM REQUIRED CONFIG)

set(add_link_deps ${add_link_deps} LLVM)

add_executable(pl0 pl0.cc)
target_link_libraries(pl0 ${add_link_deps})
target_include_directories(pl0 PUBLIC ${LLVM_INCLUDE_DIRS})
