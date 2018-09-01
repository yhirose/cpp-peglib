cmake_minimum_required(VERSION 2.8)
include_directories(..)
add_definitions("-std=c++11")
add_executable(peglint peglint.cc server.cc)

if(WIN32)
  target_link_libraries(peglint wsock32 ws2_32)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	#set(add_link_deps pthread)
  target_link_libraries(peglint pthread)
endif()
