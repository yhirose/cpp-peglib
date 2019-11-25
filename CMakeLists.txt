cmake_minimum_required(VERSION 3.1.0)
project("cpp-peglib")

# Check if a supported compiler is used to setup the C++ standard to use:
get_property(known_features GLOBAL PROPERTY CMAKE_CXX_KNOWN_FEATURES)
list(FIND known_features "cxx_std_17" found)
if(NOT ${found} EQUAL -1)
  # C++17 standard is supported
  set(CMAKE_CXX_STANDARD 17)
else()
  # Check for C++11 standard support
  list(FIND known_features "cxx_std_11" found)
  if(NOT ${found} EQUAL -1)
    # C++11 standard is supported
    set(CMAKE_CXX_STANDARD 11)
  endif()
endif()

if(${found} EQUAL -1)
  message(FATAL_ERROR "Your compiler is not supported.")
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
    OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  set(warning_options -Weverything
    -Wno-c++98-compat
    -Wno-padded
    -Wno-c++98-compat-pedantic
    -Wno-exit-time-destructors
    -Wno-missing-prototypes
    -Wno-weak-vtables
    -Wno-global-constructors
    -Wno-format-nonliteral
    -Wno-switch-enum
    -Wno-missing-noreturn
    -Wno-covered-switch-default
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(warning_options -Wall -Wpedantic -Wextra)
elseif(MSVC)
  set(warning_options /W4 /wd4503 /wd4512)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(add_link_deps pthread)
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif()

enable_testing()

add_subdirectory(test)
add_subdirectory(example)
