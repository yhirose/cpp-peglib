add_executable(benchmark benchmark.cc)
target_include_directories(benchmark PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(benchmark PRIVATE
  BENCHMARK_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_link_libraries(benchmark ${add_link_deps})

# Optional: link libpg_query for YACC comparison
find_library(PG_QUERY_LIB pg_query)
find_path(PG_QUERY_INCLUDE pg_query.h)
if(PG_QUERY_LIB AND PG_QUERY_INCLUDE)
  message(STATUS "Found libpg_query: ${PG_QUERY_LIB}")
  target_include_directories(benchmark PRIVATE ${PG_QUERY_INCLUDE})
  target_link_libraries(benchmark ${PG_QUERY_LIB})
  target_compile_definitions(benchmark PRIVATE HAS_PG_QUERY)
else()
  message(STATUS "libpg_query not found - YACC benchmark disabled")
endif()
