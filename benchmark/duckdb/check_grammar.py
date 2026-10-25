#!/usr/bin/env python3
"""Checks the grammar make_grammar.py generates from a DuckDB release against DuckDB's own parser.

  check_grammar.py <version> <duckdb source dir> [build dir]

<duckdb source dir> is a checkout of the release, for its grammar and its tests:
  git clone --depth 1 --branch v<version> https://github.com/duckdb/duckdb <duckdb source dir>
<build dir> is the build of the DuckDB comparison (default: build/duckdb), with duck_check_<version> and peg_check.

The check fails if a rule of the grammar leaves subtrees out of the AST, or if the grammar and DuckDB disagree on a
statement. The statements are those of DuckDB's tests (test/sql/**/*.test), and the statements of
benchmark/data/big.sql with one token left out or repeated, most of which are wrong. DuckDB rejects some statements
after its grammar has accepted them, in the transformer; those count as accepted. A statement with a tagged
dollar-quoted string is a known limit of the generated grammar and does not fail the check.
"""

import math
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))

TOKEN = re.compile(r"'(?:[^']|'')*'|\"(?:[^\"]|\"\")*\"|[A-Za-z_][A-Za-z0-9_]*|\d+(?:\.\d*)?|\S")
TOKENS_MUTATED_PER_STATEMENT = 25
TAGGED_DOLLAR_QUOTE = re.compile(r"\$(?:[A-Za-z_]|[^\x00-\x7f])(?:[A-Za-z0-9_]|[^\x00-\x7f])*\$")
SHOWN = 10


def test_statements(source_dir):
    """The SQL of each statement and query block of the tests, without the blocks that use test variables."""
    seen, statements = set(), []
    for dirpath, dirnames, filenames in os.walk(os.path.join(source_dir, "test", "sql")):
        dirnames.sort()
        for name in sorted(filenames):
            if not name.endswith(".test"):
                continue
            with open(os.path.join(dirpath, name), encoding="utf-8", errors="replace") as f:
                lines = f.read().split("\n")
            i = 0
            while i < len(lines):
                if not lines[i].startswith(("statement ", "query ")):
                    i += 1
                    continue
                i += 1
                body = []
                while i < len(lines) and lines[i].strip() and lines[i] != "----":
                    body.append(lines[i])
                    i += 1
                sql = "\n".join(body)
                if sql and "${" not in sql and "\0" not in sql and sql not in seen:
                    seen.add(sql)
                    statements.append(sql)
    return statements


def split_statements(text):
    """Splits SQL at the semicolons outside quotes and comments."""
    statements, start, i = [], 0, 0
    while i < len(text):
        c = text[i]
        if c in "'\"":
            i += 1
            while i < len(text) and not (text[i] == c and text[i + 1:i + 2] != c):
                i += 2 if text[i] == c else 1
        elif text.startswith("--", i):
            i = text.find("\n", i)
            i = len(text) if i < 0 else i
        elif text.startswith("/*", i):
            i = text.find("*/", i + 2)
            i = len(text) if i < 0 else i + 1
        elif c == ";":
            statements.append(text[start:i + 1])
            start = i + 1
        i += 1
    if text[start:].strip():
        statements.append(text[start:])
    return [s for s in statements if s.strip()]


def mutations(statements):
    """Each statement with one token left out, and with one token repeated, at up to 25 tokens spread over it."""
    result = []
    for sql in statements:
        spans = [m.span() for m in TOKEN.finditer(sql)]
        step = max(1, math.ceil(len(spans) / TOKENS_MUTATED_PER_STATEMENT))
        for s, e in spans[::step]:
            result.append(sql[:s] + sql[e:])
            result.append(sql[:e] + " " + sql[s:])
    return result


def verdicts(command, statements, work):
    path = os.path.join(work, "statements")
    with open(path, "wb") as f:
        f.write("\0".join(statements).encode("utf-8", errors="replace"))
    out = subprocess.run(command + [path], check=True, stdout=subprocess.PIPE, text=True).stdout.split()
    if len(out) != len(statements):
        raise SystemExit(f"{command[0]} gave {len(out)} verdicts for {len(statements)} statements")
    return out


def compare(name, statements, duck_check, peg_check, work):
    """Prints the number of disagreements and the first of those that are not a known limit, and returns how
    many are not."""
    duck = verdicts([duck_check], statements, work)
    peg = verdicts(peg_check, statements, work)
    mismatches = [i for i, (d, p) in enumerate(zip(duck, peg)) if (d == "S") != (p == "R")]
    unknown = [i for i in mismatches if not TAGGED_DOLLAR_QUOTE.search(statements[i])]
    print(f"{name}: {len(statements)} statements, {len(mismatches)} disagreements, "
          f"{len(mismatches) - len(unknown)} of them with a tagged dollar quote")
    for i in unknown[:SHOWN]:
        verdict = "rejects" if duck[i] == "S" else "accepts"
        print(f"  DuckDB {verdict}: {statements[i][:160]!r}")
    return len(unknown)


def main(argv):
    if len(argv) not in (3, 4):
        raise SystemExit(__doc__)
    version, source_dir = argv[1], argv[2]
    build_dir = argv[3] if len(argv) == 4 else os.path.join(REPO, "build", "duckdb")
    bin_dir = os.path.join(build_dir, "benchmark", "duckdb")
    duck_check = os.path.join(bin_dir, "duck_check_" + version.replace(".", "_"))
    peg_check = os.path.join(bin_dir, "peg_check")
    gram = os.path.join(source_dir, "src", "include", "duckdb", "parser", "peg", "inlined_grammar.gram")

    with tempfile.TemporaryDirectory() as work:
        grammar = os.path.join(work, "grammar.peg")
        subprocess.run([sys.executable, os.path.join(HERE, "make_grammar.py"), gram, grammar], check=True)

        dropped = subprocess.run([peg_check, "--dropped-subtrees", grammar], check=True, stdout=subprocess.PIPE,
                                 text=True).stdout.split()
        print(f"rules that leave subtrees out of the AST: {len(dropped)} {' '.join(dropped)}")

        with open(os.path.join(REPO, "benchmark", "data", "big.sql"), encoding="utf-8") as f:
            big = re.sub(r"\) AT,$", ") AX,", f.read(), flags=re.M)  # as run.sh does
        failures = len(dropped)
        failures += compare("DuckDB tests", test_statements(source_dir), duck_check, [peg_check, grammar], work)
        # big.sql repeats its statements, so each is mutated once.
        statements = list(dict.fromkeys(split_statements(big)))
        failures += compare("big.sql with a token left out or repeated", mutations(statements),
                            duck_check, [peg_check, grammar], work)
    if failures:
        raise SystemExit("check failed")
    print("ok")


if __name__ == "__main__":
    main(sys.argv)
