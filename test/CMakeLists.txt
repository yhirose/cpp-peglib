cmake_minimum_required(VERSION 3.14)
project(test)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(test-main test1.cc test2.cc test3.cc)

target_include_directories(test-main PRIVATE ..)

include(GoogleTest)
gtest_discover_tests(test-main)
target_link_libraries(test-main PRIVATE gtest_main)
