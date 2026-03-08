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

Measured on Apple M2 Max, macOS, AppleClang 17, `-O3` (Release build), 10 iterations.

cpp-peglib is approximately **7–10x slower** than the YACC parser, consistent with the findings reported in the DuckDB article.

| Benchmark | PEG/YACC |
|---|---|
| TPC-H Q1 (544 B) | 9.9x slower |
| all TPC-H (14 KB) | 7.8x slower |
| big.sql (1.2 MB) | 7.3x slower |

## First-Set Optimization

The First-Set optimization precomputes the set of possible first bytes for each `PrioritizedChoice` alternative at grammar compilation time. At parse time, alternatives whose First-Set does not include the current input byte are skipped without attempting them.

| Benchmark | PEG/YACC |
|---|---|
| TPC-H Q1 (544 B) | 5.9x slower |
| all TPC-H (14 KB) | 4.6x slower |
| big.sql (1.2 MB) | 4.6x slower |

## Devirtualization of `dynamic_cast` in Holder

`Holder::parse_core` previously used 2–3 `dynamic_cast` calls per rule match to check whether the inner operator is a `TokenBoundary`, `PrioritizedChoice`, or `Dictionary`. These RTTI lookups accounted for ~27% of parse time in profiling. Replacing them with boolean flags (`is_token_boundary`, `is_choice_like`) on the `Ope` base class eliminates the RTTI overhead entirely.

| Benchmark | PEG/YACC |
|---|---|
| TPC-H Q1 (544 B) | 4.2x slower |
| all TPC-H (14 KB) | 3.4x slower |
| big.sql (1.2 MB) | 3.4x slower |

## Left Recursion Support

Left recursion support adds `DetectLeftRecursion` and seed-growing logic at parse time. For non-left-recursive grammars (such as SQL), this adds zero overhead — only a single `bool` check per rule invocation.

| Benchmark | PEG/YACC |
|---|---|
| TPC-H Q1 (544 B) | 4.3x slower |
| all TPC-H (14 KB) | 3.7x slower |
| big.sql (1.2 MB) | 3.4x slower |

No regression compared to the previous configuration.

## Summary (big.sql, ~1.2 MB)

All optimizations measured on Apple M2 Max, macOS, AppleClang 17, `-O3` (Release build).

| Configuration | Median | PEG/YACC |
|---|---|---|
| YACC (libpg_query) | 31.2 ms | 1.0x |
| PEG (no optimizations) | 228.4 ms | 7.4x |
| PEG + Devirt | 190.9 ms | 6.2x |
| PEG + First-Set | 135.8 ms | 4.6x |
| PEG (all opts + LR support) | 107.4 ms | 3.4x |

```
YACC                         |████                                   31.2 ms (1.0x)
PEG (all opts + LR support)  |██████████████                        107.4 ms (3.4x)
PEG + First-Set              |█████████████████                     135.8 ms (4.6x)
PEG + Devirt                 |████████████████████████              190.9 ms (6.2x)
PEG (no optimizations)       |█████████████████████████████         228.4 ms (7.4x)
```

With all optimizations including left recursion support, the gap to YACC is **3.4x**.
