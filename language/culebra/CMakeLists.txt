cmake_minimum_required(VERSION 2.8)
include_directories(../..)

if(MSVC)
  add_compile_options(${cxx11_options} /W3)
  add_definitions(-DUNICODE)
else()
  add_compile_options(${cxx11_options})
endif()

add_executable(culebra main.cc)
