cmake_minimum_required(VERSION 3.14)
project(cpp-peglib)

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
# Fix found at https://github.com/ethereum/solidity/pull/13429
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus /utf-8 /D_CRT_SECURE_NO_DEPRECATE")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

option(PEGLIB_BUILD_TESTS "Build cpp-peglib tests" ON)
option(PEGLIB_BUILD_LINT "Build cpp-peglib lint" ON)
option(PEGLIB_BUILD_EXAMPLE "Build cpp-peglib examples" ON)

if(${PEGLIB_BUILD_LINT})
add_subdirectory(lint)
endif()

if(${PEGLIB_BUILD_EXAMPLE})
add_subdirectory(example)
# add_subdirectory(cymbol)
endif()

if (${PEGLIB_BUILD_TESTS})
  add_subdirectory(test)
  enable_testing()
endif()

install(FILES peglib.h DESTINATION include)
