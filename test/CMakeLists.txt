cmake_minimum_required(VERSION 3.14)
project(test)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(peglib-test-main test1.cc test2.cc test3.cc)

target_include_directories(peglib-test-main PRIVATE ..)

include(GoogleTest)
gtest_discover_tests(peglib-test-main)
target_link_libraries(peglib-test-main PRIVATE gtest_main)
