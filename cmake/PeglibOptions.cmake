include_guard(GLOBAL)

# Only default ot building tests and examples when building standalone, which
# we can determine by whether the top of the build tree is this folder or not.
if(CMAKE_SOURCE_DIR STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  set(_standalone_build ON)
else()
  set(_standalone_build OFF)
endif()

option(PEGLIB_BUILD_LINTER   "Build peglib's linter"  ${_standalone_build})
option(PEGLIB_BUILD_TESTS    "Build cpp-peglib tests" ${_standalone_build})
option(PEGLIB_BUILD_EXAMPLES "Build example parsers"  ${_standalone_build})

unset(_standalone_build)
