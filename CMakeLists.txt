cmake_minimum_required(VERSION 3.1.0)
project(cpp-peglib)

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

##############
# Installation paths
set(INCLUDE_INSTALL_DIR include/ CACHE PATH "Installation directory for C++ headers")
set(LIB_INSTALL_DIR lib${LIB_SUFFIX}/ CACHE PATH "Installation directory for libraries")

add_subdirectory(example)
add_subdirectory(lint)

add_subdirectory(test)
enable_testing()

install(FILES peglib.h DESTINATION include)

##########
# Generate -config.cmake file
# This file will allow to link to this library (peglib.h) with cmake find_package

# Install destination to -config.cmake
set(INSTALL_DESTINATION_PATH ${LIB_INSTALL_DIR}/cmake/${PROJECT_NAME}${MSVCARCH_DIR_EXTENSION_EXT})

include(CMakePackageConfigHelpers)

configure_package_config_file(${PROJECT_SOURCE_DIR}/cmake/packaging/Config.cmake.in
    ${PROJECT_BINARY_DIR}/cmake/config/${PROJECT_NAME}-config.cmake
    INSTALL_DESTINATION ${INSTALL_DESTINATION_PATH}
    PATH_VARS INCLUDE_INSTALL_DIR
)
install(FILES ${PROJECT_BINARY_DIR}/cmake/config/${PROJECT_NAME}-config.cmake
    DESTINATION ${INSTALL_DESTINATION_PATH}
    COMPONENT cmake
)
