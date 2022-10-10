include_guard(GLOBAL)

# We don't really have any dependencies, except needing to link against the
# pthreads library on linu.x
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(THREADS_PREFER_PTHREAD_FLAG ON)

  find_package(Threads REQUIRED)

  target_compile_options(CppPegLib "-pthread")
  target_link_libraries(CppPegLib Threads::Threads)
endif()

