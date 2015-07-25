cmake_minimum_required(VERSION 3.0)
include_directories(..)
add_definitions("-std=c++1y" -DPEGLIB_HAS_CONSTEXPR_SUPPORT)

add_executable(culebra main.cc)
