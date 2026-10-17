# cpp-peglib SQL Benchmark

This benchmark compares cpp-peglib's PEG parser against PostgreSQL's YACC-based parser (via [libpg_query](https://github.com/pganalyze/libpg_query)) using SQL workloads from the [DuckDB PEG parser experiments](https://github.com/hannes/peg-parser-experiments).

## Background

A [DuckDB blog post](https://duckdb.org/2024/11/22/runtime-extensible-parsers.html) reported that cpp-peglib is roughly 10x slower than the YACC-based parser used in PostgreSQL for SQL parsing. This benchmark reproduces that comparison to establish a baseline and track performance improvements over time.

## Test Data

All test data comes from the [peg-parser-experiments](https://github.com/hannes/peg-parser-experiments) repository, except `sql-optimized.peg`:

| File | Description | Size |
| --- | --- | --- |
| `sql.peg` | PEG grammar for SQL (covers TPC-H and TPC-DS) | 3.9 KB |
| `sql-optimized.peg` | `sql.peg` rewritten to parse faster | 4.4 KB |
| `q1.sql` | Single TPC-H query (Q1) | 544 B |
| `all-tpch.sql` | All 22 TPC-H queries | 14 KB |
| `big.sql` | TPC-H + TPC-DS queries repeated 6x | 1.2 MB |

## Building

```bash
# macOS, for the YACC cases: brew install libpg_query
cmake -B build -DBUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target benchmark
```

If `libpg_query` is installed, the YACC cases are enabled. Otherwise they are skipped.

## Running

```bash
./build/benchmark/benchmark [iterations]
```

Default is 10 iterations. Each case reports median, mean, min, and max times in milliseconds, and with libpg_query, the ratios of cases 3 and 4 to cases 8 and 9.

```bash
./build/benchmark/benchmark profile [q1|tpch] [opt]
```

Parses big.sql (or Q1, or all of TPC-H) once and shows how many times each rule succeeds and fails and the time spent in it, not counting the rules it calls, and its packrat cache hits and misses, with `sql-optimized.peg` if `opt` is given.

## Benchmark Cases

The PEG cases parse with packrat parsing enabled.

1. **PEG: TPC-H Q1**: Parse a single SQL query with cpp-peglib
2. **PEG: all TPC-H**: Parse all 22 TPC-H queries with cpp-peglib
3. **PEG: big.sql**: Parse the ~1MB SQL file with cpp-peglib
4. **PEG-ast: big.sql**: The same, building an AST
5. **PEG-opt: big.sql**: The same as 3, with `sql-optimized.peg`
6. **YACC: TPC-H Q1**: Parse a single SQL query with libpg_query (PostgreSQL YACC)
7. **YACC: all TPC-H**: Parse all 22 TPC-H queries with libpg_query
8. **YACC: big.sql**: Parse the ~1MB SQL file with libpg_query
9. **YACC-parse: big.sql**: The same, without converting the parse tree to JSON

`pg_query_parse()`, which the YACC cases 6 to 8 call, also converts the whole parse tree to JSON, which no PEG case does. So the YACC case to compare with is 9, which builds a tree as case 4 does; case 3 builds none.
