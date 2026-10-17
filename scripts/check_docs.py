#!/usr/bin/env python3
"""Checks the examples in the README files against the code.

Each `> peglint ...` line in a code block must print the lines that follow
it, given the files that `> cat` lines and `# name.peg` blocks show before
it. Each ```cpp block must compile, unless the line before it is
`<!-- not compiled -->`.

usage: check_docs.py --peglint PATH [--cxx COMPILER]
"""

import argparse
import difflib
import os
import re
import shlex
import signal
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ["README.md", "lint/README.md"]
NOT_COMPILED = "<!-- not compiled -->"


def code_blocks(lines):
    """Yields (line number of the first line, language, lines, compiled)."""
    i = 0
    while i < len(lines):
        fence = re.match(r"^```(\S*)", lines[i])
        if not fence:
            i += 1
            continue
        end = i + 1
        while end < len(lines) and not lines[end].startswith("```"):
            end += 1
        compiled = i == 0 or lines[i - 1].strip() != NOT_COMPILED
        yield i + 2, fence.group(1), lines[i + 1 : end], compiled
        i = end + 1


def commands(block):
    """Yields (index, command, the lines it prints) for each `> command`."""
    starts = [i for i, line in enumerate(block) if line.startswith("> ")]
    for start, end in zip(starts, starts[1:] + [len(block)]):
        output = block[start + 1 : end]
        while output and not output[-1].strip():
            output.pop()
        yield start, block[start][2:], output


def run_peglint(peglint, args, cwd):
    """Returns what peglint prints, leaving out stdout redirected with `>`."""
    redirected = ">" in args
    if redirected:
        args = args[: args.index(">")]
    result = subprocess.run(
        [peglint] + args, cwd=cwd, text=True,
        stdout=subprocess.DEVNULL if redirected else subprocess.PIPE,
        stderr=subprocess.PIPE if redirected else subprocess.STDOUT)
    if result.returncode < 0:  # a crash prints nothing of its own
        return f"peglint: {signal.Signals(-result.returncode).name}"
    return result.stderr if redirected else result.stdout


def check_transcripts(doc, blocks, peglint, tmp):
    failures = []
    for line, language, block, _ in blocks:
        named = re.match(r"^# (\S+\.peg)$", block[0]) if block else None
        if language == "peg" and named:
            Path(tmp, named.group(1)).write_text("\n".join(block) + "\n")
        for index, command, output in commands(block):
            args = shlex.split(command)
            if args[0] == "cat":
                Path(tmp, args[1]).write_text("\n".join(output) + "\n")
            elif args[0] in ("peglint", "./peglint"):
                printed = run_peglint(peglint, args[1:], tmp).rstrip()
                expected = [s.rstrip() for s in output]
                actual = [s.rstrip() for s in printed.splitlines()]
                if actual != expected:
                    diff = difflib.unified_diff(expected, actual, "expected",
                                                "actual", lineterm="")
                    failures.append(f"{doc}:{line + index}: > {command}\n" +
                                    "\n".join(diff))
    return failures


# The head of a function definition, up to the `{` that ends its line.
FUNCTION = re.compile(
    r"^[A-Za-z_][\w:<>, ]*[ *&]+[\w:]+\([^;=\[]*\)( const)? \{$", re.M)


def check_cpp(doc, blocks, cxx, tmp):
    programs, fragments = [], []
    for line, language, block, compiled in blocks:
        if language != "cpp" or not compiled:
            continue
        text = f'#line {line} "{doc}"\n' + "\n".join(block) + "\n"
        if re.search(r"\bint main\s*\(", text):
            programs.append(text)
        elif FUNCTION.search(text):
            fragments.append(f"namespace example_{line} {{\n{text}}}\n")
        else:
            # Statements, given a parser and a grammar text unless they make
            # their own.
            fragments.append(f"void example_{line}() {{\n"
                             "  peg::parser parser;\n  std::string grammar;\n"
                             f"  {{\n{text}  }}\n}}\n")
    prelude = ("#include <peglib.h>\n#include <iostream>\n"
               "using namespace peg;\nusing namespace std;\n")
    sources = programs + ([prelude + "\n".join(fragments)] if fragments
                          else [])
    failures = []
    for i, source in enumerate(sources):
        path = Path(tmp, f"example{i}.cc")
        path.write_text(source)
        result = subprocess.run(
            [cxx, "-std=c++17", "-fsyntax-only", f"-I{ROOT}", str(path)],
            text=True, capture_output=True)
        if result.returncode:
            failures.append(result.stderr)
    return failures


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--peglint", required=True)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    args = parser.parse_args()
    peglint = str(Path(args.peglint).resolve())

    failures = []
    for doc in DOCS:
        blocks = list(code_blocks((ROOT / doc).read_text().split("\n")))
        with tempfile.TemporaryDirectory() as tmp:
            failures += check_transcripts(doc, blocks, peglint, tmp)
            failures += check_cpp(doc, blocks, args.cxx, tmp)
    for failure in failures:
        print(failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
