#!/bin/bash
# Compares DuckDB releases (versions.txt) with cpp-peglib on one input.
#
#   benchmark/duckdb/run.sh [build_dir] [rounds] [iterations]
#
# The input is benchmark/data/big.sql with the alias AT renamed to AX, because DuckDB reserves AT.
# Rounds run the programs interleaved; each program's time is the minimum over the rounds.
# Peak memory comes from separate processes that keep the parse result, the median of three.
#
# DuckDB 1.x parses with YACC and 2.x and later with PEG, so the rows say so by major version.
# cpp-peglib parses with the grammar make_grammar.py generates from the PEG grammar of each 2.x and
# later version (fetched by fetch.sh), without and with its lookaheads.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
build=${1:-$repo/build/duckdb}
rounds=${2:-3}
iterations=${3:-5}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
sed -E 's/\) AT,$/) AX,/' "$repo/benchmark/data/big.sql" > "$work/big_ax.sql"

versions=()
while read -r version || [ -n "$version" ]; do
  case "$version" in
    ""|\#*) continue ;;
  esac
  versions+=("$version")
done < "$here/versions.txt"
if [ ${#versions[@]} = 0 ]; then
  echo "no version in $here/versions.txt" >&2
  exit 1
fi

# One row per DuckDB version, then one per cpp-peglib grammar.
names=()
kinds=()
targets=()
row() {
  names+=("$1")
  kinds+=("$2")
  targets+=("$3")
}
# A release before 2.0 parses with YACC.
is_yacc() { [ "${1%%.*}" -lt 2 ]; }
for version in "${versions[@]}"; do
  if is_yacc "$version"; then parser=YACC; else parser=PEG; fi
  row "DuckDB $version ($parser)" duck "duck_extract_${version//./_}"
done
for version in "${versions[@]}"; do
  if is_yacc "$version"; then continue; fi
  gram=$here/work/cache/$version/inlined_grammar.gram
  if [ ! -f "$gram" ]; then
    echo "$gram not found: run benchmark/duckdb/fetch.sh first" >&2
    exit 1
  fi
  python3 "$here/make_grammar.py" "$gram" "$work/duckdb-$version.peg"
  python3 "$here/make_grammar.py" --lookaheads "$gram" "$work/duckdb-$version-lookaheads.peg"
  row "cpp-peglib (DuckDB $version)" peg "$work/duckdb-$version.peg"
  row "cpp-peglib (DuckDB $version) +lookaheads" peg "$work/duckdb-$version-lookaheads.peg"
done

# run <row> <argument>: the argument is the iteration count, or "rss".
run() {
  local input=$work/big_ax.sql
  if [ "${kinds[$1]}" = duck ]; then
    "$build/benchmark/duckdb/${targets[$1]}" "$input" "$2"
  else
    "$build/benchmark/duckdb/peg_parse" "${targets[$1]}" "$input" "$2"
  fi
}

count=${#names[@]}
echo "load average: $(uptime | sed 's/.*load averages*: //')"

mins=(); counts=()
for ((i = 0; i < count; i++)); do
  mins[i]=1e9
  counts[i]=""
done

for ((r = 1; r <= rounds; r++)); do
  for ((i = 0; i < count; i++)); do
    out=$(run "$i" "$iterations") # stops here when a program fails
    read -r min _ n <<< "$out"
    mins[i]=$(awk -v a="${mins[i]}" -v b="$min" 'BEGIN { print (a < b) ? a : b }')
    counts[i]="${counts[i]} $n"
  done
done

echo
printf "%-40s %12s %12s %12s\n" "" "min ms" "peak MB" "statements"
for ((i = 0; i < count; i++)); do
  mem=$(for _ in 1 2 3; do run "$i" rss | awk '{print $1}'; done | sort -n | sed -n 2p)
  printf "%-40s %12.1f %12.1f %12s\n" "${names[i]}" "${mins[i]}" "$mem" \
    "$(echo "${counts[i]}" | tr ' ' '\n' | sort -u | grep . | tr '\n' ' ')"
done

# Every program must split and parse the same statements, or the times compare different work.
if [ "$(echo "${counts[*]}" | tr ' ' '\n' | sort -u | grep -c .)" != 1 ]; then
  for ((i = 0; i < count; i++)); do
    echo "statement counts: ${names[i]}:${counts[i]}" >&2
  done
  echo "statement counts differ" >&2
  exit 1
fi
