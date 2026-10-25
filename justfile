set shell := ["bash", "-cu"]

default:
    @just --list

release *args:
    @./scripts/release.sh {{args}}

clean:
    rm -rf build build_* benchmark/duckdb/work

test:
    cmake --build build -j {{num_cpus()}} --target peglib-test-main
    build/test/peglib-test-main
    CPPPEGLIB_TEST_BLOB_ROUNDTRIP=1 build/test/peglib-test-main

# Compares with DuckDB (macOS): fetches its libraries, builds and runs benchmark/duckdb/run.sh.
# args: [rounds] [iterations]
duckdb-bench *args:
    benchmark/duckdb/fetch.sh
    cmake -B build/duckdb -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
    cmake --build build/duckdb --parallel {{num_cpus()}}
    benchmark/duckdb/run.sh build/duckdb {{args}}

bench:
    #!/usr/bin/env bash
    set -euo pipefail

    # Build
    cmake --build build --target benchmark >/dev/null 2>&1

    # Run cpp-peglib
    cpp_out=$(build/benchmark/benchmark 10 2>&1)

    # Extract medians (field after "median:")
    extract() { echo "$1" | grep "$2" | grep "median:" | head -1 | sed 's/.*median: *\([0-9.]*\) ms.*/\1/'; }

    cpp_q1=$(extract "$cpp_out" "PEG: TPC-H Q1")
    cpp_tpch=$(extract "$cpp_out" "PEG: all TPC-H")
    cpp_big=$(extract "$cpp_out" "PEG: big.sql")
    cpp_ast=$(extract "$cpp_out" "PEG-ast: big.sql")
    cpp_opt=$(extract "$cpp_out" "PEG-opt: big.sql")
    yacc_q1=$(extract "$cpp_out" "YACC: TPC-H Q1")
    yacc_tpch=$(extract "$cpp_out" "YACC: all TPC-H")
    yacc_big=$(extract "$cpp_out" "YACC: big.sql")
    yacc_parse=$(extract "$cpp_out" "YACC-parse: big.sql")

    # Ratios on big.sql vs YACC without the JSON conversion, which no PEG case
    # does. YACC builds a tree, as cpp-peglib (AST) does; the others only
    # recognize the input.
    ratio_ast=$(echo "scale=1; $cpp_ast / $yacc_parse" | bc)
    ratio_cpp=$(echo "scale=1; $cpp_big / $yacc_parse" | bc)
    ratio_opt=$(echo "scale=1; $cpp_opt / $yacc_parse" | bc)

    # Bar widths (max 40 chars, scaled to slowest)
    max=$cpp_ast
    bar() {
        local val=$1 width=$(echo "scale=0; $1 * 40 / $max" | bc)
        printf '%*s' "$width" '' | tr ' ' '█'
    }

    echo ""
    echo "================================================================"
    echo " PEG SQL Benchmark (n=10, median ms)"
    echo "================================================================"
    printf "%-28s %8s %8s %8s\n" "" "TPC-H Q1" "all TPC-H" "big.sql"
    echo "----------------------------------------------------------------"
    printf "%-28s %8s %8s %8s\n" "cpp-peglib" "$cpp_q1" "$cpp_tpch" "$cpp_big"
    printf "%-28s %8s %8s %8s\n" "cpp-peglib (optimized)" "" "" "$cpp_opt"
    printf "%-28s %8s %8s %8s\n" "YACC (libpg_query, + JSON)" "$yacc_q1" "$yacc_tpch" "$yacc_big"
    printf "%-28s %8s %8s %8s\n" "YACC (libpg_query)" "" "" "$yacc_parse"
    echo ""
    echo " big.sql:"
    printf "  YACC              %6s ms |%-40s| 1.0x\n" "$yacc_parse" "$(bar $yacc_parse)"
    printf "  cpp-peglib (opt)  %6s ms |%-40s| ${ratio_opt}x\n" "$cpp_opt" "$(bar $cpp_opt)"
    printf "  cpp-peglib (AST)  %6s ms |%-40s| ${ratio_ast}x\n" "$cpp_ast" "$(bar $cpp_ast)"
    printf "  cpp-peglib        %6s ms |%-40s| ${ratio_cpp}x\n" "$cpp_big" "$(bar $cpp_big)"
    echo "================================================================"
