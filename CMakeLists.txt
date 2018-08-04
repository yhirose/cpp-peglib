cmake_minimum_required(VERSION 2.8)

# Check if a supported compiler is used and add c++11 flag:
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
    message(FATAL_ERROR "Need at least gcc 4.9 to compile.")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(MSVC)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19)
    message(FATAL_ERROR "Visual Studio 2015 or newer is required.")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
    message(FATAL_ERROR "Need at least AppleClang 7.0 to compile.")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.4)
    message(FATAL_ERROR "Clang below version 3.4 will most likely not work. Please upgrade your compiler.")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else() # no GNU, no MSVC, no Clang
  message(WARNING "You are using an unsupported compiler. Compilation has only been tested with MSVC, GCC and Clang.")

  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag(-std=c++11 HAS_CXX11_FLAG)
  if(HAS_CXX11_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  else()
    message(FATAL_ERROR "Your compiler doesn't support the '-std=c++11' flag.")
  endif()
endif()


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
