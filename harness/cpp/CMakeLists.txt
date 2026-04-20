cmake_minimum_required(VERSION 3.14)
project(peg-spec-harness)

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

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# Path to peglib.h (two levels up from harness/cpp/)
set(PEGLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Path to spec/tests/
set(SPEC_TESTS_DIR ${PEGLIB_DIR}/spec/tests)

add_executable(spec-harness harness.cc)

target_include_directories(spec-harness PRIVATE ${PEGLIB_DIR})

target_compile_definitions(spec-harness PRIVATE
  SPEC_TESTS_DIR="${SPEC_TESTS_DIR}"
)

target_link_libraries(spec-harness PRIVATE
  gtest_main
  nlohmann_json::nlohmann_json
  ${add_link_deps}
)

include(GoogleTest)
gtest_discover_tests(spec-harness)

enable_testing()
