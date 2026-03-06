# cpp-peglib SQL Benchmark

This benchmark compares cpp-peglib's PEG parser against PostgreSQL's YACC-based parser (via [libpg_query](https://github.com/pganalyze/libpg_query)) using SQL workloads from the [DuckDB PEG parser experiments](https://github.com/hannes/peg-parser-experiments).

## Background

A [DuckDB blog post](https://duckdb.org/2024/11/22/runtime-extensible-parsers.html) reported that cpp-peglib is roughly 10x slower than the YACC-based parser used in PostgreSQL for SQL parsing. This benchmark reproduces that comparison to establish a baseline and track performance improvements over time.

## Test Data

All test data comes from the [peg-parser-experiments](https://github.com/hannes/peg-parser-experiments) repository:

| File | Description | Size |
|---|---|---|
| `sql.gram` | PEG grammar for SQL (covers TPC-H and TPC-DS) | 3.9 KB |
| `q1.sql` | Single TPC-H query (Q1) | 544 B |
| `all-tpch.sql` | All 22 TPC-H queries | 14 KB |
| `big.sql` | TPC-H + TPC-DS queries repeated 6x | 1.2 MB |

## Building

```bash
# Without libpg_query (PEG benchmarks only)
cmake -B build -DBUILD_TESTS=OFF -DPEGLIB_BUILD_BENCHMARK=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target benchmark

# With libpg_query (PEG vs YACC comparison)
# macOS: brew install libpg_query
cmake -B build -DBUILD_TESTS=OFF -DPEGLIB_BUILD_BENCHMARK=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target benchmark
```

If `libpg_query` is installed, the YACC benchmarks are automatically enabled. Otherwise they are skipped.

## Running

```bash
./build/benchmark/benchmark [iterations]
```

Default is 10 iterations. Each benchmark reports median, mean, min, and max times in milliseconds.

## Benchmark Cases

1. **PEG: grammar load** — Time to construct a cpp-peglib parser from the SQL PEG grammar
2. **PEG: TPC-H Q1** — Parse a single SQL query with cpp-peglib
3. **PEG: all TPC-H** — Parse all 22 TPC-H queries with cpp-peglib
4. **PEG: big.sql** — Parse the ~1MB SQL file with cpp-peglib
5. **YACC: TPC-H Q1** — Parse a single SQL query with libpg_query (PostgreSQL YACC)
6. **YACC: all TPC-H** — Parse all 22 TPC-H queries with libpg_query
7. **YACC: big.sql** — Parse the ~1MB SQL file with libpg_query

## Baseline Performance

Measured on Apple M2 Max, macOS, AppleClang 17, `-O3` (Release build), 10 iterations:

```
Summary:
  PEG: grammar load        median:      3.539 ms
  PEG: TPC-H Q1            median:      0.138 ms
  PEG: all TPC-H           median:      2.880 ms
  PEG: big.sql (~1MB)      median:    228.433 ms
  YACC: TPC-H Q1           median:      0.014 ms
  YACC: all TPC-H          median:      0.368 ms
  YACC: big.sql (~1MB)     median:     31.130 ms
```

| Benchmark | cpp-peglib (PEG) | PostgreSQL YACC | Ratio |
|---|---|---|---|
| TPC-H Q1 (544 B) | 0.138 ms | 0.014 ms | 9.9x |
| all TPC-H (14 KB) | 2.880 ms | 0.368 ms | 7.8x |
| big.sql (1.2 MB) | 228.4 ms | 31.1 ms | 7.3x |

cpp-peglib is approximately **7–10x slower** than the YACC parser, consistent with the findings reported in the DuckDB article.
