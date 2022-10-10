/Users/yhirose/Projects/cpp-peglib/.travis.yml
project(pl0)

add_executable(pl0 pl0.cc)
find_package(LLVM REQUIRED CONFIG)
target_include_directories(pl0 PUBLIC ${LLVM_INCLUDE_DIRS})
target_link_libraries(pl0 CppPegLib LLVM)
