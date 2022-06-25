cmake_minimum_required(VERSION 3.14)
project(cpp-peglib)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus /utf-8")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

option(PEGLIB_BUILD_TESTS "Build cpp-peglib tests" OFF)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(add_link_deps Threads::Threads)
endif()

add_subdirectory(example)
add_subdirectory(lint)

if (${PEGLIB_BUILD_TESTS})
  add_subdirectory(test)
  enable_testing()
endif()

install(FILES peglib.h DESTINATION include)
