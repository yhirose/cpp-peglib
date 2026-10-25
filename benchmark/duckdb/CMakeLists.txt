# Comparison with DuckDB. Needs the libraries from fetch.sh (macOS only for now); without them
# nothing here is built.
#
# The programs that link DuckDB are built once per version listed in versions.txt, because the
# symbols of two versions clash in one binary. Their names are duck_extract_<version> and, from
# 2.0 on, duck_check_<version>, with the dots of the version as underscores.

set(DUCKDB_CACHE ${CMAKE_CURRENT_SOURCE_DIR}/work/cache)
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/versions.txt DUCKDB_VERSION_LINES)

set(DUCKDB_VERSIONS "")
foreach(line IN LISTS DUCKDB_VERSION_LINES)
  string(STRIP "${line}" version)
  if(NOT (version STREQUAL "" OR version MATCHES "^#"))
    list(APPEND DUCKDB_VERSIONS ${version})
  endif()
endforeach()

foreach(version IN LISTS DUCKDB_VERSIONS)
  if(NOT EXISTS ${DUCKDB_CACHE}/${version}/libduckdb.dylib)
    message(STATUS "libduckdb ${version} not found - DuckDB comparison disabled (run benchmark/duckdb/fetch.sh)")
    return()
  endif()
endforeach()

foreach(version IN LISTS DUCKDB_VERSIONS)
  set(dir ${DUCKDB_CACHE}/${version})
  string(REPLACE "." "_" suffix ${version})
  set(programs duck_extract)
  # duck_check classifies the errors of the PEG parser, which DuckDB has from 2.0 on.
  if(version VERSION_GREATER_EQUAL 2.0)
    list(APPEND programs duck_check)
  endif()
  foreach(program IN LISTS programs)
    set(target ${program}_${suffix})
    add_executable(${target} ${program}.cc)
    target_include_directories(${target} PRIVATE ${dir})
    target_link_libraries(${target} ${dir}/libduckdb.dylib)
    set_target_properties(${target} PROPERTIES BUILD_RPATH ${dir})
  endforeach()
endforeach()

foreach(program peg_parse peg_check)
  add_executable(${program} ${program}.cc)
  target_include_directories(${program} PRIVATE ${CMAKE_SOURCE_DIR})
  target_link_libraries(${program} ${add_link_deps})
endforeach()
