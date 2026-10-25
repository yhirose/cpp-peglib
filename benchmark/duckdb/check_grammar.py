#!/usr/bin/env python3
"""Checks the grammar make_grammar.py generates from a DuckDB release against DuckDB's own parser.

  check_grammar.py <version> <duckdb source dir> [build dir]

<duckdb source dir> is a checkout of the release, for its grammar and its tests:
  git clone --depth 1 --branch v<version> https://github.com/duckdb/duckdb <duckdb source dir>
<build dir> is the build of the DuckDB comparison (default: build/duckdb), with duck_check_<version> and peg_check.

The check fails if a rule of the grammar leaves subtrees out of the AST, or if the grammar and DuckDB disagree on a
statement. It does so for the grammar with lookaheads (make_grammar.py --lookaheads) too, which must also accept
the same statements and build the same trees as the grammar without them. The statements are those of DuckDB's tests (test/sql/**/*.test), a few around how the tokenizer reads
'$', and the statements of benchmark/data/big.sql with one token left out or repeated, most of which are wrong.
DuckDB rejects some statements after its grammar has accepted them, in the transformer; those count as accepted.
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
NO_BREAK_SPACE = chr(0x00A0)
# How the tokenizer reads '$': a numbered or a named parameter, or a dollar-quoted string, closed or not, which
# DuckDB's tests do not all cover.
DOLLAR_STATEMENTS = [
    "SELECT $x$abc;", "SELECT $1;", "SELECT $abc;", "SELECT $x$a$x$;", "SELECT $x$a$y$;", "SELECT $1$a$1$;",
    "SELECT $_a1$x$_a1$;", "SELECT $a-b$x$a-b$;", "SELECT $$;", "SELECT $$a$$;", "SELECT $x$a$$x$;",
    "SELECT x$y$z;", "SELECT $é$a$é$;", "SELECT $x$a$x$ AS $y$b$y$;", "SELECT $x$a$x$$y$;", "SELECT $x$\n$x$;",
    "SELECT $;", "SELECT $ x$;", "SELECT 1 $x$;", "SELECT $x $x$;", "SELECT $x$ab$x;", "SELECT $X$a$x$;",
    "SELECT $x$a$X$ || $X$b$X$;", "SELECT $a$1$a$::INT;", "SELECT $1a$x$1a$;", "PREPARE p AS SELECT $abc;",
    # Names in type modifiers, where a string can start in no name: an E-string and a dollar quote only look like one.
    "SELECT DECIMAL(date'\\') '1';", "SELECT DECIMAL(a$t$) '$t$';", "SELECT DECIMAL(a$$) '$$';",
    # A unicode space in a tag, which DuckDB does not turn into a space.
    f"SELECT $a{NO_BREAK_SPACE}b$x$a{NO_BREAK_SPACE}b$;", f"SELECT $a{NO_BREAK_SPACE}b$x;",
]
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
    out = subprocess.run(command + [path], check=True, stdout=subprocess.PIPE, text=True).stdout.splitlines()
    if len(out) != len(statements):
        raise SystemExit(f"{command[0]} gave {len(out)} verdicts for {len(statements)} statements")
    return out


def compare(name, statements, duck_check, peg_check, grammar, lookahead_grammar, work):
    """Prints the disagreements with DuckDB of the grammar, and the differences between the grammar and the one with
    lookaheads, in the verdict or the tree, and returns the number of both."""
    duck = verdicts([duck_check], statements, work)
    plain = verdicts([peg_check, "--trees", grammar], statements, work)
    ahead = verdicts([peg_check, "--trees", lookahead_grammar], statements, work)
    mismatches = [i for i, (d, p) in enumerate(zip(duck, plain)) if (d[0] == "S") != (p[0] == "R")]
    differences = [i for i, (p, a) in enumerate(zip(plain, ahead)) if p != a]
    print(f"{name}: {len(statements)} statements, {len(mismatches)} disagreements with DuckDB, "
          f"{len(differences)} differences with lookaheads")
    for i in mismatches[:SHOWN]:
        verdict = "rejects" if duck[i][0] == "S" else "accepts"
        print(f"  DuckDB {verdict}: {statements[i][:160]!r}")
    for i in differences[:SHOWN]:
        print(f"  lookaheads change the result: {statements[i][:160]!r}")
    return len(mismatches) + len(differences)


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
        lookahead_grammar = os.path.join(work, "grammar_lookaheads.peg")
        for path, flags in ((grammar, []), (lookahead_grammar, ["--lookaheads"])):
            subprocess.run([sys.executable, os.path.join(HERE, "make_grammar.py"), *flags, gram, path], check=True)

        dropped = []
        for g in (grammar, lookahead_grammar):
            dropped += subprocess.run([peg_check, "--dropped-subtrees", g], check=True, stdout=subprocess.PIPE,
                                      text=True).stdout.split()
        print(f"rules that leave subtrees out of the AST: {len(dropped)} {' '.join(dropped)}")

        with open(os.path.join(REPO, "benchmark", "data", "big.sql"), encoding="utf-8") as f:
            big = re.sub(r"\) AT,$", ") AX,", f.read(), flags=re.M)  # as run.sh does
        # big.sql repeats its statements, so each is mutated once.
        big_statements = list(dict.fromkeys(split_statements(big)))
        sets = [("DuckDB tests", test_statements(source_dir)),
                ("statements around '$'", DOLLAR_STATEMENTS),
                ("big.sql with a token left out or repeated", mutations(big_statements))]
        failures = len(dropped)
        for name, statements in sets:
            failures += compare(name, statements, duck_check, peg_check, grammar, lookahead_grammar, work)
    if failures:
        raise SystemExit("check failed")
    print("ok")


if __name__ == "__main__":
    main(sys.argv)
