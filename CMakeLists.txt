cmake_minimum_required(VERSION 3.0)
add_definitions("-std=c++1y")

add_subdirectory(lint)
add_subdirectory(test)
add_subdirectory(example)
add_subdirectory(language/pl0)
add_subdirectory(language/culebra)
