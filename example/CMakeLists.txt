cmake_minimum_required(VERSION 2.8)
include_directories(..)

if(MSVC)
  add_compile_options(${cxx11_options} /W3)
else()
  add_compile_options(${cxx11_options})
endif()

add_executable(calc calc.cc)
target_link_libraries(calc ${add_link_deps})

add_executable(calc2 calc2.cc)
target_link_libraries(calc2 ${add_link_deps})

add_executable(calc3 calc3.cc)
target_link_libraries(calc3 ${add_link_deps})
