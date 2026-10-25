# DuckDB comparison

This benchmark compares the time and peak memory of cpp-peglib with DuckDB's own parsers, on the same input:

| Row | What is measured |
| --- | --- |
| DuckDB X.Y.Z (YACC) | `duckdb_extract_statements` of a 1.x release, which parses with DuckDB's YACC parser |
| DuckDB X.Y.Z (PEG) | `duckdb_extract_statements` of a 2.x or later release, which parses with DuckDB's PEG parser |
| cpp-peglib (DuckDB X.Y.Z) | the grammar `make_grammar.py` generates from the PEG grammar of a 2.x or later version (see below) |
| cpp-peglib (DuckDB X.Y.Z) +lookaheads | the same grammar with a lookahead in front of the rules that are tried everywhere and almost always fail (see below) |

Each program parses the whole input in one call and builds the result: the extracted statements for DuckDB, and the AST for cpp-peglib (`enable_ast(true)` with packrat parsing). The time includes freeing the result.

## Running

```bash
just duckdb-bench [rounds] [iterations]   # defaults: 3 rounds, 5 iterations
```

It runs the steps below, which can also be run by hand:

```bash
benchmark/duckdb/fetch.sh                  # downloads the DuckDB libraries and grammars of versions.txt
cmake -B build/duckdb -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
cmake --build build/duckdb --parallel 8
benchmark/duckdb/run.sh build/duckdb [rounds] [iterations]
```

The libraries and grammars are not part of the repository. They are stored in `benchmark/duckdb/work/`, which is ignored by git. The programs are built with the other benchmarks (`PEGLIB_BUILD_BENCHMARK`) once the libraries of all versions in `versions.txt` are there, and only on macOS for now. Without them CMake prints a message and skips this directory.

## Changing the DuckDB versions

The DuckDB versions to compare are listed in `versions.txt`, one per line. When DuckDB releases a new version, change the line, then run `just duckdb-bench` again. `fetch.sh` finds the macOS library of each version through the GitHub release API, so no version-specific code is needed.

The cpp-peglib grammar of a 2.x or later version is generated from that version's grammar, so it follows the change: `run.sh` generates it at each run, in a temporary directory, from the grammar `fetch.sh` stores in `work/cache/<version>/`. The hand-made grammars in `benchmark/data/` are copies of DuckDB 2.0.0's grammar, made before the generator, and `run.sh` does not use them any more: a few of their rules leave subtrees out of the AST (`peg_check --dropped-subtrees` lists them) and they do not read comments.

## The generated grammar

`make_grammar.py` writes DuckDB's PEG grammar in cpp-peglib's syntax. DuckDB matches its grammar over the tokens of its tokenizer, and replaces some rules with matchers written in C++. The script writes both into the grammar: a literal matches a whole token, ignoring case; an identifier rule takes the keywords of the categories DuckDB allows there; and whitespace, comments, numbers, strings and operators are read as DuckDB's tokenizer reads them. Outside lookaheads, only terminal rules use a token boundary, because cpp-peglib builds a rule that has one as a single AST node and leaves the subtrees under it out.

One rule, `DollarQuotedString`, is not in the grammar: `peg_parse` and `peg_check` give it as a C++ function (`dollar_quote.h`). It reads a dollar-quoted string such as `$tag$...$tag$`, which ends where its opening tag comes again. The grammar could match that only with a backreference, and a capture keeps packrat parsing from breaking the cycles of the expression rules. The function takes the input alone, so cpp-peglib parses as fast as without it.

The generated grammar differs from DuckDB in these cases:

- A keyword directly followed by `$` or by a non-ASCII letter is read as the keyword, where DuckDB reads one word.
- DuckDB turns unicode spaces into spaces before it tokenizes, but not after a `$` that this step takes for the start of a dollar-quoted string that is never closed, as in `x$y$z`, or right after a `$`. DuckDB reads such a unicode space as part of a word, and the grammar as a space.
- `:` also matches the first character of `::` and `:=`, which DuckDB reads as one token, so `a[1::2]` is accepted where DuckDB rejects it.
- DuckDB rejects some statements after its grammar has accepted them, while it builds the statement, for example a positional reference `#0`. The grammar accepts these.

With `--lookaheads`, `make_grammar.py` puts a lookahead in front of 21 rules: the qualified names, the type literal, `*`, the function call, and the aliases with a colon. Each lookahead is a necessary condition for its rule to match, so that a failed alternative costs a short scan instead of a rule, with the same trees and verdicts. The rules are those that profiling a hand-made port of DuckDB 2.0.0's grammar showed to be tried at almost every position.

`check_grammar.py` checks the generated grammar against DuckDB's own parser. It needs a checkout of the release, for its grammar and its tests:

```bash
git clone --depth 1 --branch v2.0.0 https://github.com/duckdb/duckdb benchmark/duckdb/work/duckdb-2.0.0
python3 benchmark/duckdb/check_grammar.py 2.0.0 benchmark/duckdb/work/duckdb-2.0.0 [build/duckdb]
```

It fails if a rule leaves subtrees out of the AST, if the grammar with lookaheads accepts another statement than the grammar without them or builds another tree, or if the grammar and DuckDB disagree on a statement of DuckDB's tests, on a few statements around how DuckDB's tokenizer reads `$`, or on a statement of the input with one token left out or repeated. A statement that DuckDB rejects only while it builds the statement counts as accepted.

## Input

The input is `benchmark/data/big.sql` with the alias `AT` renamed to `AX`, because DuckDB reserves `AT` and cannot parse the original file. The script makes this copy at run time, so the repository keeps one copy of the data. All programs parse the renamed input, so they parse the same statements. `run.sh` fails if the statement counts differ.

## Method

- The programs run interleaved, round by round. Each run does one warm-up and then the timed iterations, and reports their minimum. The script takes the minimum over the rounds.
- Peak memory comes from separate processes that keep the parse result. The median of three is reported. Memory varies by about 10% between runs of the same program.
- Run it on an otherwise idle machine. The load average is printed before the results, so a loaded machine can be noticed.
- The times are wall-clock times of one process on one thread. The numbers depend on the machine and on the DuckDB release, so compare the rows within one run.
- The times include tokenizing, parsing, building the result, and freeing it. DuckDB's result is a typed statement tree; cpp-peglib's result is a generic AST. The two trees are not the same, so the rows show the cost of each complete path, not the cost of one identical step.
