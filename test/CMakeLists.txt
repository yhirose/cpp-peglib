cmake_minimum_required(VERSION 3.14)
project(test)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.0.0-preview3
)
FetchContent_MakeAvailable(Catch2)

set(CMAKE_CXX_STANDARD 17)

add_executable(test-main test1.cc test2.cc test3.cc)

target_include_directories(test-main PRIVATE ..)
target_link_libraries(test-main PRIVATE Catch2::Catch2WithMain)

enable_testing()

add_test(
  NAME TestMain
  COMMAND test-main
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
