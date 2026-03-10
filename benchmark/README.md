# cpp-peglib SQL Benchmark

This benchmark compares cpp-peglib's PEG parser against PostgreSQL's YACC-based parser (via [libpg_query](https://github.com/pganalyze/libpg_query)) using SQL workloads from the [DuckDB PEG parser experiments](https://github.com/hannes/peg-parser-experiments).

## Background

A [DuckDB blog post](https://duckdb.org/2024/11/22/runtime-extensible-parsers.html) reported that cpp-peglib is roughly 10x slower than the YACC-based parser used in PostgreSQL for SQL parsing. This benchmark reproduces that comparison to establish a baseline and track performance improvements over time.

## Test Data

All test data comes from the [peg-parser-experiments](https://github.com/hannes/peg-parser-experiments) repository:

| File | Description | Size |
| --- | --- | --- |
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
| --- | --- |
| TPC-H Q1 (544 B) | 9.9x slower |
| all TPC-H (14 KB) | 7.8x slower |
| big.sql (1.2 MB) | 7.3x slower |

## First-Set Optimization

The First-Set optimization precomputes the set of possible first bytes for each `PrioritizedChoice` alternative at grammar compilation time. At parse time, alternatives whose First-Set does not include the current input byte are skipped without attempting them.

| Benchmark | PEG/YACC |
| --- | --- |
| TPC-H Q1 (544 B) | 5.9x slower |
| all TPC-H (14 KB) | 4.6x slower |
| big.sql (1.2 MB) | 4.6x slower |

## Devirtualization of `dynamic_cast` in Holder

`Holder::parse_core` previously used 2–3 `dynamic_cast` calls per rule match to check whether the inner operator is a `TokenBoundary`, `PrioritizedChoice`, or `Dictionary`. These RTTI lookups accounted for ~27% of parse time in profiling. Replacing them with boolean flags (`is_token_boundary`, `is_choice_like`) on the `Ope` base class eliminates the RTTI overhead entirely.

| Benchmark | PEG/YACC |
| --- | --- |
| TPC-H Q1 (544 B) | 4.2x slower |
| all TPC-H (14 KB) | 3.4x slower |
| big.sql (1.2 MB) | 3.4x slower |

## Left Recursion Support

Left recursion support adds `DetectLeftRecursion` and seed-growing logic at parse time. For non-left-recursive grammars (such as SQL), this adds zero overhead — only a single `bool` check per rule invocation.

| Benchmark | PEG/YACC |
| --- | --- |
| TPC-H Q1 (544 B) | 4.3x slower |
| all TPC-H (14 KB) | 3.7x slower |
| big.sql (1.2 MB) | 3.4x slower |

No regression compared to the previous configuration.

## ISpan Optimization (Repetition + CharacterClass Fusion)

At grammar compilation time, `Repetition` nodes whose child is an ASCII-only `CharacterClass` are detected. At parse time, these use a tight bitset-test loop instead of the full operator dispatch chain (vtable call, `push`/`pop`, `decode_codepoint`, `scope_exit`, etc.). This is equivalent to LPeg's `ISpan` instruction.

A/B comparison (same session, alternating builds):

| Benchmark | Baseline | ISpan | Improvement |
| --- | --- | --- | --- |
| TPC-H Q1 (544 B) | 0.088 ms | 0.077 ms | -12.5% |
| all TPC-H (14 KB) | 1.489 ms | 1.409 ms | -5.4% |
| big.sql (1.2 MB) | 126.0 ms | 114.6 ms | -9.1% |

| Benchmark | PEG/YACC |
| --- | --- |
| TPC-H Q1 (544 B) | 5.1x slower |
| all TPC-H (14 KB) | 3.7x slower |
| big.sql (1.2 MB) | 3.7x slower |

Note: Grammar load time increases slightly (~0.8 ms) due to bitset construction, but this is a one-time cost at grammar compilation.

## Snapshot/Rollback (Phase 2)

Replaced the `push()`/`pop()`/`append()` pattern with LPeg-style snapshot/rollback. Instead of allocating a child `SemanticValues` scope and copying results on success, operators now write directly to the parent and truncate on failure. `CaptureScope` was flattened from `vector<map>` to a flat `vector<pair>` with reverse linear search.

Key changes:

- `PrioritizedChoice`: snapshot before each alternative, rollback on failure, no-op on success
- `Sequence`: direct write to parent (no child scope)
- `Repetition`: snapshot only when `max` is bounded
- `AndPredicate`/`NotPredicate`: always rollback (side-effect isolation)
- `CaptureScope`: flat `vector<pair<string_view, string>>` instead of scoped `vector<map>`

A/B comparison (same session, alternating builds):

| Benchmark | Baseline | Snapshot/Rollback | Improvement |
| --- | --- | --- | --- |
| TPC-H Q1 (544 B) | 0.075 ms | 0.058 ms | -22.7% |
| all TPC-H (14 KB) | 1.286 ms | 1.161 ms | -9.7% |
| big.sql (1.2 MB) | 105.4 ms | 99.2 ms | -5.9% |

| Benchmark | PEG/YACC |
| --- | --- |
| TPC-H Q1 (544 B) | 4.1x slower |
| all TPC-H (14 KB) | 3.2x slower |
| big.sql (1.2 MB) | 3.4x slower |

The improvement is most pronounced on small inputs (Q1: -22.7%) where per-rule allocation overhead dominates, and smaller on large inputs (big.sql: -5.9%) where the grammar structure itself is the bottleneck.

## Keyword Guard (Phase 3)

At grammar compilation time, the pattern `!ReservedKeyword <[a-z_]i[a-z0-9_]i*>` is detected. At parse time, instead of running the full NotPredicate → Holder → PrioritizedChoice chain for each keyword alternative, the fast path scans the identifier using a bitset, then checks the result against a precomputed keyword table. Identifiers whose length falls outside the keyword length range skip the lookup entirely.

Key techniques:

- Bitset-based identifier scanning (same as ISpan)
- Stack buffer for case-folding (heap fallback for identifiers > 64 chars)
- Length-range early-out (`min_keyword_len` / `max_keyword_len`)
- Compound keywords (e.g., `GROUP BY`) fall back to the normal path

A/B comparison (same session, alternating builds):

| Benchmark | Baseline | Keyword Guard | Improvement |
| --- | --- | --- | --- |
| TPC-H Q1 (544 B) | 0.058 ms | 0.055 ms | -5.2% |
| all TPC-H (14 KB) | 1.117 ms | 1.109 ms | -0.7% |
| big.sql (1.2 MB) | 99.2 ms | 92.4 ms | -6.8% |

| Benchmark | PEG/YACC |
| --- | --- |
| TPC-H Q1 (544 B) | 3.7x slower |
| all TPC-H (14 KB) | 3.0x slower |
| big.sql (1.2 MB) | 3.1x slower |

## Whitespace Skip Optimization

At grammar compilation time, `Sequence` nodes with whitespace operators between elements are detected. At parse time, instead of dispatching through the full operator chain for each whitespace consumption, a fast inline function scans whitespace using a precomputed bitset. This eliminates vtable calls, scope management, and SemanticValues bookkeeping for one of the most frequently invoked operations.

A/B comparison (same session, alternating builds):

| Benchmark | Baseline | Whitespace Skip | Improvement |
| --- | --- | --- | --- |
| big.sql (1.2 MB) | 92.4 ms | 93.0 ms | ~neutral |

| Benchmark | PEG/YACC |
| --- | --- |
| big.sql (1.2 MB) | 3.0x slower |

The improvement was within noise range on big.sql. The optimization primarily benefits grammars with heavy whitespace-separated sequences.

## Selective Packrat Memoization

At grammar compilation time, static analysis identifies which rules actually benefit from packrat memoization. A rule benefits only if it is reachable from 2+ alternatives of the same `PrioritizedChoice` (i.e., backtracking will re-visit it at the same position). Rules that don't benefit use a lightweight bitvector-only re-entry guard instead of the full `std::map`-based cache.

Empirical profiling of the SQL grammar showed that only 2 of 53 rules benefit from packrat (Identifier: 50.3% hit rate, ColumnReference: 45.1%). The remaining 51 rules had 0% hit rate with ~295K wasted map insertions.

A/B comparison (same session, alternating builds):

| Benchmark | Baseline | Selective Packrat | Improvement |
| --- | --- | --- | --- |
| big.sql (1.2 MB) | 93.0 ms | 88.3 ms | -5.1% |

| Benchmark | PEG/YACC |
| --- | --- |
| TPC-H Q1 (544 B) | 3.8x slower |
| all TPC-H (14 KB) | 2.8x slower |
| big.sql (1.2 MB) | 2.8x slower |

## Summary (big.sql, ~1.2 MB)

All optimizations measured on Apple M2 Max, macOS, AppleClang 17, `-O3` (Release build).

| Configuration | Median | PEG/YACC |
| --- | --- | --- |
| YACC (libpg_query) | 31.2 ms | 1.0x |
| PEG (no optimizations) | 228.4 ms | 7.4x |
| PEG + Devirt | 190.9 ms | 6.2x |
| PEG + First-Set | 135.8 ms | 4.6x |
| PEG + First-Set + Devirt + LR | 107.4 ms | 3.4x |
| PEG (all opts + Snapshot/Rollback) | 99.2 ms | 3.4x |
| PEG (all opts + Keyword Guard) | 92.4 ms | 3.1x |
| PEG (all opts + Selective Packrat) | 88.3 ms | 2.8x |

```ascii
YACC                       |████                            31.2 ms (1.0x)
PEG (all opts + Sel. Pack) |███████████                     88.3 ms (2.8x)
PEG (all opts + KW Guard)  |████████████                    92.4 ms (3.1x)
PEG (all opts + S/R)       |█████████████                   99.2 ms (3.4x)
PEG + First-Set + Devirt   |██████████████                 107.4 ms (3.4x)
PEG + First-Set            |█████████████████              135.8 ms (4.6x)
PEG + Devirt               |████████████████████████       190.9 ms (6.2x)
PEG (no optimizations)     |█████████████████████████████  228.4 ms (7.4x)
```

With all optimizations including Selective Packrat, the gap to YACC is **2.8x** on big.sql — a **2.6x improvement** from the original 7.4x baseline.
