# Peglib is a header-only library with zero dependencies (pthreads on Linux).
#
# To build with it, add the CppPegLib INTERFACE in your cmake lists, this will
# provide the neccessary include paths, linkage, etc.
#
# Then your code simply needs to #include <peglib.h>

cmake_minimum_required(VERSION 3.14)

project(cpp-peglib DESCRIPTION "C++17 header-only PEG library" LANGUAGES CXX)

include(cmake/PeglibCXXStandard.cmake)
include(cmake/PeglibOptions.cmake)

add_library(
  CppPegLib

  INTERFACE

  peglib.h
)

# Tag the library with the path to the top of it's source tree.
set_target_properties(CppPegLib PROPERTIES SOURCE_FOLDER ${CMAKE_CURRENT_LIST_DIR})

# Include cmake helpers that attach other properties to the target.
include(cmake/PeglibCompileOptions.cmake)
include(cmake/PeglibIncludePaths.cmake)
include(cmake/PeglibThreads.cmake)


if(PEGLIB_BUILD_LINTER)
  add_subdirectory(lint)
else()
  message(STATUS "PEGLIB_BUILD_LINTER is OFF: Not building linter")
endif()


if(PEGLIB_BUILD_EXAMPLES)
  add_subdirectory(example)
else()
  message(STATUS "PEGLIB_BUILD_EXAMPLES is OFF: Not building examples")
endif ()

# add_subdirectory(cymbol)


if (PEGLIB_BUILD_TESTS)
  include(CTest)
  add_subdirectory(test)
  enable_testing()
else()
  message(STATUS "PEGLIB_BUILD_TESTS is OFF: Skipping tests")
endif()


# This facilitates users who want to install this system-wide, but they'll have the
# complication of needing to set the compile flags and potentially link flag.
# Might be worth telling CMake how to generate a config when installing.
install(FILES peglib.h DESTINATION include)
