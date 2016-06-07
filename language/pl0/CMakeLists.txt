cmake_minimum_required(VERSION 2.8)
include_directories(../..)

if(MSVC)
  add_compile_options(${cxx11_options} /W0)
else()
  add_compile_options(${cxx11_options})
endif()

add_executable(pl0 pl0.cc)
