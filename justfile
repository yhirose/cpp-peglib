set shell := ["bash", "-cu"]

default:
    @just --list

release *args:
    @./scripts/release.sh {{args}}

test:
    cmake --build build -j {{num_cpus()}} --target peglib-test-main
    build/test/peglib-test-main
    CPPPEGLIB_TEST_BLOB_ROUNDTRIP=1 build/test/peglib-test-main

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
    cpp_opt=$(extract "$cpp_out" "PEG-opt: big.sql")
    yacc_q1=$(extract "$cpp_out" "YACC: TPC-H Q1")
    yacc_tpch=$(extract "$cpp_out" "YACC: all TPC-H")
    yacc_big=$(extract "$cpp_out" "YACC: big.sql")

    # Ratios vs YACC (big.sql)
    ratio_cpp=$(echo "scale=1; $cpp_big / $yacc_big" | bc)
    ratio_opt=$(echo "scale=1; $cpp_opt / $yacc_big" | bc)

    # Bar widths (max 40 chars, scaled to slowest)
    max=$cpp_big
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
    printf "%-28s %8s %8s %8s\n" "YACC (libpg_query)" "$yacc_q1" "$yacc_tpch" "$yacc_big"
    echo ""
    echo " big.sql:"
    printf "  YACC              %6s ms |%-40s| 1.0x\n" "$yacc_big" "$(bar $yacc_big)"
    printf "  cpp-peglib (opt)  %6s ms |%-40s| ${ratio_opt}x\n" "$cpp_opt" "$(bar $cpp_opt)"
    printf "  cpp-peglib        %6s ms |%-40s| ${ratio_cpp}x\n" "$cpp_big" "$(bar $cpp_big)"
    echo "================================================================"
