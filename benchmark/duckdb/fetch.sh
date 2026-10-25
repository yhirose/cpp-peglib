#!/bin/bash
# Downloads the DuckDB libraries listed in versions.txt (macOS) from the GitHub releases into
# benchmark/duckdb/work/cache/<version>/, which is where CMakeLists.txt looks for them, and the
# PEG grammar of each release from 2.0 on, which run.sh turns into a cpp-peglib grammar.
# The files are not part of the repository. Run again after editing versions.txt.
set -euo pipefail

cd "$(dirname "$0")"
cache=work/cache
mkdir -p "$cache"

fetch() {
  local version=$1
  local dir="$cache/$version"
  if [ -f "$dir/libduckdb.dylib" ]; then
    echo "already fetched: $version"
    return
  fi

  # The release page lists the macOS library under one of these names, depending on the release.
  local url
  url=$(curl -fsSL "https://api.github.com/repos/duckdb/duckdb/releases/tags/v$version" |
    python3 -c '
import json, sys
assets = {a["name"]: a["browser_download_url"] for a in json.load(sys.stdin)["assets"]}
for name in ["libduckdb-osx-universal.zip", "duckdb-shared-libs-osx-universal.tar.gz",
             "duckdb-shared-libs-osx-arm64.tar.gz"]:
    if name in assets:
        print(assets[name])
        break
')
  if [ -z "$url" ]; then
    echo "no macOS library asset for DuckDB $version" >&2
    exit 1
  fi

  mkdir -p "$dir"
  local archive="$cache/$version.archive"
  echo "fetching DuckDB $version: $url"
  curl -fL --retry 3 -o "$archive" "$url"
  case "$url" in
    *.zip) unzip -oq "$archive" -d "$dir" ;;
    *.tar.gz) tar xzf "$archive" -C "$dir" ;;
  esac
  rm -f "$archive"
  if [ ! -f "$dir/libduckdb.dylib" ]; then
    echo "libduckdb.dylib not found in the archive for DuckDB $version" >&2
    exit 1
  fi
}

# The PEG grammar of a release (2.0 and later), from which run.sh generates the cpp-peglib grammar.
fetch_grammar() {
  local version=$1
  local file="$cache/$version/inlined_grammar.gram"
  if [ "${version%%.*}" -lt 2 ] || [ -f "$file" ]; then
    return
  fi
  echo "fetching the PEG grammar of DuckDB $version"
  # Through another name, so that a download cut off is not taken for the file.
  curl -fsSL --retry 3 -o "$file.part" \
    "https://raw.githubusercontent.com/duckdb/duckdb/v$version/src/include/duckdb/parser/peg/inlined_grammar.gram"
  mv "$file.part" "$file"
}

fetched=""
while read -r version || [ -n "$version" ]; do
  case "$version" in
    ""|\#*) continue ;;
  esac
  fetch "$version"
  fetch_grammar "$version"
  fetched="$fetched $version"
done < versions.txt

echo "fetched:$fetched"
