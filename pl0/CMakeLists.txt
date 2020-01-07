cmake_minimum_required(VERSION 2.8)
include_directories(..)

if(MSVC)
  add_compile_options(${cxx11_options} /W3)
else()
  add_compile_options(${cxx11_options})
endif()

option(BUILD_PL0_WITH_JIT "Build with jit compiler" OFF)

add_executable(pl0 pl0.cc)

if (BUILD_PL0_WITH_JIT)
    find_package(LLVM REQUIRED CONFIG)
    set(add_link_deps ${add_link_deps} LLVM)
    target_link_libraries(pl0 ${add_link_deps})
    target_include_directories(pl0 PUBLIC ${LLVM_INCLUDE_DIRS})
    add_definitions(-DWITH_JIT)

endif()
