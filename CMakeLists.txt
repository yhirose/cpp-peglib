cmake_minimum_required(VERSION 3.1.0)
project("cpp-peglib")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus /utf-8")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(add_link_deps Threads::Threads)
endif()

add_subdirectory(example)
add_subdirectory(lint)

add_subdirectory(test)
enable_testing()

install(FILES peglib.h DESTINATION include)
