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

set(PEGLIB_TEST_SOURCES
  test_core.cc
  test_peg_syntax.cc
  test_rules.cc
  test_error.cc
  test_token_predicate.cc
  test_unicode_state.cc
  test_packrat.cc
  test_left_recursive.cc
  test_first_set.cc
  test_integration.cc
  test_trace.cc
  test_combinators.cc
  test_definition_api.cc
  test_utf8.cc
  test_snapshot.cc
  test_mini_js.cc
  test_serialize.cc
)

include(GoogleTest)

# peglib-test-blob-roundtrip runs the same tests with every grammar reloaded
# from its serialized blob (CPPPEGLIB_TEST_BLOB_ROUNDTRIP in peglib.h).
foreach(target peglib-test-main peglib-test-blob-roundtrip)
  add_executable(${target} ${PEGLIB_TEST_SOURCES})
  target_include_directories(${target} PRIVATE ..)
  # Pass the absolute source path of the mini-js grammar so the test can locate
  # it regardless of the working directory it is run from (e.g. nested build
  # trees or package builds).
  target_compile_definitions(${target} PRIVATE
    MINI_JS_GRAMMAR_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../spec/mini-js/grammar.peg")
  target_link_libraries(${target} PRIVATE gtest_main)
endforeach()

target_compile_definitions(peglib-test-blob-roundtrip PRIVATE
  CPPPEGLIB_TEST_BLOB_ROUNDTRIP)

gtest_discover_tests(peglib-test-main)
gtest_discover_tests(peglib-test-blob-roundtrip TEST_PREFIX "BlobRoundTrip.")
