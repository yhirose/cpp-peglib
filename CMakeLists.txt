cmake_minimum_required(VERSION 3.14)
project(peglib)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus /utf-8 /D_CRT_SECURE_NO_DEPRECATE")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(add_link_deps Threads::Threads)
endif()

add_library(peglib INTERFACE)
target_include_directories(peglib INTERFACE ${CMAKE_SOURCE_DIR})

option(BUILD_TESTS "Build cpp-peglib tests" ON)
option(PEGLIB_BUILD_LINT "Build cpp-peglib lint utility" OFF)
option(PEGLIB_BUILD_EXAMPLES "Build cpp-peglib examples" OFF)

if (${BUILD_TESTS})
  add_subdirectory(test)
  enable_testing()
endif()

if (${PEGLIB_BUILD_LINT})
  add_subdirectory(lint)
endif()

if (${PEGLIB_BUILD_EXAMPLES})
  add_subdirectory(example)
endif()

install(FILES peglib.h DESTINATION include)
