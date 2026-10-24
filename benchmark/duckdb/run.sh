#!/bin/bash
# Compares DuckDB releases (versions.txt) with cpp-peglib (three grammars) on one input.
#
#   benchmark/duckdb/run.sh [build_dir] [rounds] [iterations]
#
# The input is benchmark/data/big.sql with the alias AT renamed to AX, because DuckDB reserves AT.
# Rounds run the programs interleaved; each program's time is the minimum over the rounds.
# Peak memory comes from separate processes that keep the parse result, the median of three.
#
# DuckDB 1.x parses with YACC and 2.x and later with PEG, so the rows say so by major version.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
build=${1:-$repo/build/duckdb}
rounds=${2:-3}
iterations=${3:-5}
data=$repo/benchmark/data

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
sed -E 's/\) AT,$/) AX,/' "$data/big.sql" > "$work/big_ax.sql"

# One row per DuckDB version, then one per cpp-peglib grammar.
names=()
kinds=()
targets=()
while read -r version; do
  case "$version" in
    ""|\#*) continue ;;
  esac
  major=${version%%.*}
  if [ "$major" = 1 ]; then parser=YACC; else parser=PEG; fi
  names+=("DuckDB $version ($parser)")
  kinds+=(duck)
  targets+=("duck_extract_${version//./_}")
done < "$here/versions.txt"
for grammar in duckdb-2.0.0.peg duckdb-2.0.0-optimized.peg duckdb-2.0.0-optimized-dict.peg; do
  case "$grammar" in
    duckdb-2.0.0.peg) label="cpp-peglib" ;;
    duckdb-2.0.0-optimized.peg) label="cpp-peglib optimized" ;;
    *) label="cpp-peglib optimized+dict" ;;
  esac
  names+=("$label")
  kinds+=(peg)
  targets+=("$data/$grammar")
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
    read -r min _ n <<< "$(run "$i" "$iterations")"
    mins[i]=$(awk -v a="${mins[i]}" -v b="$min" 'BEGIN { print (a < b) ? a : b }')
    counts[i]="${counts[i]} $n"
  done
done

echo
printf "%-30s %12s %12s %12s\n" "" "min ms" "peak MB" "statements"
for ((i = 0; i < count; i++)); do
  mem=$(for _ in 1 2 3; do run "$i" rss | awk '{print $1}'; done | sort -n | sed -n 2p)
  printf "%-30s %12.1f %12.1f %12s\n" "${names[i]}" "${mins[i]}" "$mem" \
    "$(echo "${counts[i]}" | tr ' ' '\n' | sort -u | grep . | tr '\n' ' ')"
done

# Every program must split and parse the same statements, or the times compare different work.
for ((i = 0; i < count; i++)); do
  if [ "$(echo "${counts[i]}" | tr ' ' '\n' | sort -u | grep -c .)" != 1 ]; then
    echo "statement counts differ: ${names[i]}:${counts[i]}" >&2
    exit 1
  fi
done
