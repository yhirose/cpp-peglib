# DuckDB comparison

This benchmark compares the time and peak memory of cpp-peglib with DuckDB's own parsers, on the same input:

| Row | What is measured |
| --- | --- |
| DuckDB X.Y.Z (YACC) | `duckdb_extract_statements` of a 1.x release, which parses with DuckDB's YACC parser |
| DuckDB X.Y.Z (PEG) | `duckdb_extract_statements` of a 2.x or later release, which parses with DuckDB's PEG parser |
| cpp-peglib | `benchmark/data/duckdb-2.0.0.peg`, a port of DuckDB 2.0.0's SQL grammar |
| cpp-peglib optimized | `duckdb-2.0.0-optimized.peg`, the same grammar rewritten to parse faster, with the same error positions |
| cpp-peglib optimized+dict | `duckdb-2.0.0-optimized-dict.peg`, the optimized grammar with keyword lists as dictionaries |

Each program parses the whole input in one call and builds the result: the extracted statements for DuckDB, and the AST for cpp-peglib (`enable_ast(true)` with packrat parsing). The time includes freeing the result.

## Running

```bash
just duckdb-bench [rounds] [iterations]   # defaults: 3 rounds, 5 iterations
```

It runs the steps below, which can also be run by hand:

```bash
benchmark/duckdb/fetch.sh                  # downloads the DuckDB libraries listed in versions.txt
cmake -B build/duckdb -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
cmake --build build/duckdb --parallel 8
benchmark/duckdb/run.sh build/duckdb [rounds] [iterations]
```

The libraries are not part of the repository. They are stored in `benchmark/duckdb/work/`, which is ignored by git. The programs are built with the other benchmarks (`PEGLIB_BUILD_BENCHMARK`) once the libraries of all versions in `versions.txt` are there, and only on macOS for now. Without them CMake prints a message and skips this directory.

## Changing the DuckDB versions

The DuckDB versions to compare are listed in `versions.txt`, one per line. When DuckDB releases a new version, change the line, then run `just duckdb-bench` again. `fetch.sh` finds the macOS library of each version through the GitHub release API, so no version-specific code is needed.

Upgrading the cpp-peglib grammar is a separate step. The three `.peg` files in `benchmark/data/` are copies of DuckDB 2.0.0's grammar, made by hand. They are not generated from the DuckDB source, so a new DuckDB version does not change them.

## Input

The input is `benchmark/data/big.sql` with the alias `AT` renamed to `AX`, because DuckDB reserves `AT` and cannot parse the original file. The script makes this copy at run time, so the repository keeps one copy of the data. All programs parse the renamed input, so they parse the same statements. `run.sh` fails if the statement counts differ.

## Method

- The programs run interleaved, round by round. Each run does one warm-up and then the timed iterations, and reports their minimum. The script takes the minimum over the rounds.
- Peak memory comes from separate processes that keep the parse result. The median of three is reported. Memory varies by about 10% between runs of the same program.
- Run it on an otherwise idle machine. The load average is printed before the results, so a loaded machine can be noticed.
- The times are wall-clock times of one process on one thread. The numbers depend on the machine and on the DuckDB release, so compare the rows within one run.
- The times include tokenizing, parsing, building the result, and freeing it. DuckDB's result is a typed statement tree; cpp-peglib's result is a generic AST. The two trees are not the same, so the rows show the cost of each complete path, not the cost of one identical step.
