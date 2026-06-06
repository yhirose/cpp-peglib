#!/usr/bin/env bash
#
# Release a new version of cpp-peglib.
#
# Usage: ./release.sh [--run] [--minor | --major]
#
# By default, runs in dry-run mode (no changes made).
# Pass --run to actually update files, commit, tag, and push.
# Pass --minor to force a minor bump even when the ABI is unchanged
# (use this for behavioral breaking changes that don't break ABI).
# Pass --major to force a major bump.
#
# This script:
#   1. Reads the current version from peglib.h
#   2. Checks that the working directory is clean
#   3. Verifies CI status of the latest commit (all must pass except abidiff)
#   4. Determines the next version automatically:
#        - abidiff passed  → patch bump (e.g., 1.9.1 → 1.9.2)
#        - abidiff failed  → minor bump (e.g., 1.9.1 → 1.10.0)
#        - --minor passed  → forces minor bump regardless of abidiff
#        - --major passed  → forces major bump (e.g., 1.9.1 → 2.0.0)
#   5. Updates peglib.h
#   6. Commits, tags (vX.Y.Z), and pushes

set -euo pipefail

cd "$(dirname "$0")/.."

DRY_RUN=1
FORCE_MINOR=0
FORCE_MAJOR=0
while [ $# -gt 0 ]; do
  case "$1" in
    --run)
      DRY_RUN=0
      shift
      ;;
    --minor)
      FORCE_MINOR=1
      shift
      ;;
    --major)
      FORCE_MAJOR=1
      shift
      ;;
    *)
      echo "Usage: $0 [--run] [--minor | --major]"
      exit 1
      ;;
  esac
done

# --- Step 1: Read current version from peglib.h ---
CURRENT_VERSION=$(sed -n 's/^#define CPPPEGLIB_VERSION "\([^"]*\)"/\1/p' peglib.h)
IFS='.' read -r V_MAJOR V_MINOR V_PATCH <<< "$CURRENT_VERSION"

echo "==> Current version: $CURRENT_VERSION"

# --- Step 2: Check working directory is clean ---
if [ -n "$(git status --porcelain)" ]; then
  echo "Error: working directory is not clean"
  exit 1
fi

# --- Step 3: Check CI status of the latest commit ---
echo ""
echo "==> Checking CI status of the latest commit..."

HEAD_SHA=$(git rev-parse HEAD)
HEAD_SHORT=$(git rev-parse --short HEAD)
echo "    Latest commit: $HEAD_SHORT"

# Fetch all workflow runs for the HEAD commit
RUNS=$(gh run list --commit "$HEAD_SHA" --json name,status,conclusion,headSha)

NUM_RUNS=$(echo "$RUNS" | jq 'length')

if [ "$NUM_RUNS" -eq 0 ]; then
  echo "Error: No CI runs found for commit $HEAD_SHORT."
  echo "       Wait for CI to complete before releasing."
  exit 1
fi

echo "    Found $NUM_RUNS workflow run(s):"

FAILED=0
RUNNING=0
ABIDIFF_FOUND=0
ABIDIFF_PASSED=0
while IFS=$'\t' read -r name status conclusion; do
  # A run that hasn't completed yet has an empty conclusion; don't treat it
  # as a failure — the release should wait until CI finishes.
  if [ "$status" != "completed" ]; then
    echo "      [ .. ] $name (still running)"
    RUNNING=1
    continue
  fi

  if [[ "$name" == *abidiff* ]] || [[ "$name" == *abi* && "$name" != *stability* ]]; then
    ABIDIFF_FOUND=1
    if [ "$conclusion" = "success" ]; then
      echo "      [ OK ] $name"
      ABIDIFF_PASSED=1
    else
      echo "      [FAIL] $name ($conclusion) → ABI break detected, minor bump"
      ABIDIFF_PASSED=0
    fi
    continue
  fi

  if [ "$conclusion" = "success" ]; then
    echo "      [ OK ] $name"
  else
    echo "      [FAIL] $name ($conclusion)"
    FAILED=1
  fi
done < <(echo "$RUNS" | jq -r '.[] | [.name, .status, .conclusion] | @tsv')

if [ "$RUNNING" -eq 1 ]; then
  echo ""
  echo "Error: Some CI checks are still running. Wait for them to complete before releasing."
  exit 1
fi

if [ "$FAILED" -eq 1 ]; then
  echo ""
  echo "Error: Some CI checks failed. Fix them before releasing."
  exit 1
fi

echo "    All non-abidiff CI checks passed."

if [ "$ABIDIFF_FOUND" -eq 0 ]; then
  echo ""
  echo "Warning: No abidiff run found for this commit; assuming ABI unchanged."
  echo "         Use --minor or --major to override the bump type."
  ABIDIFF_PASSED=1
fi

# --- Step 4: Determine new version ---
if [ "$FORCE_MAJOR" -eq 1 ]; then
  NEW_MAJOR=$((V_MAJOR + 1))
  NEW_VERSION="$NEW_MAJOR.0.0"
  echo ""
  echo "==> --major specified → forced major bump"
elif [ "$FORCE_MINOR" -eq 1 ]; then
  NEW_MINOR=$((V_MINOR + 1))
  NEW_VERSION="$V_MAJOR.$NEW_MINOR.0"
  echo ""
  if [ "$ABIDIFF_PASSED" -eq 1 ]; then
    echo "==> abidiff passed but --minor specified → forced minor bump"
  else
    echo "==> abidiff failed → minor bump (--minor also specified)"
  fi
elif [ "$ABIDIFF_PASSED" -eq 1 ]; then
  NEW_PATCH=$((V_PATCH + 1))
  NEW_VERSION="$V_MAJOR.$V_MINOR.$NEW_PATCH"
  echo ""
  echo "==> abidiff passed → patch bump"
else
  NEW_MINOR=$((V_MINOR + 1))
  NEW_VERSION="$V_MAJOR.$NEW_MINOR.0"
  echo ""
  echo "==> abidiff failed → minor bump"
fi

IFS='.' read -r N_MAJOR N_MINOR N_PATCH <<< "$NEW_VERSION"
VERSION_HEX=$(printf "0x%02x%02x%02x" "$N_MAJOR" "$N_MINOR" "$N_PATCH")

if [ "$DRY_RUN" -eq 1 ]; then
  echo "==> [DRY RUN] New version: $NEW_VERSION ($VERSION_HEX)"
else
  echo "==> New version: $NEW_VERSION ($VERSION_HEX)"
fi

# --- Step 5: Update files ---
echo ""
if [ "$DRY_RUN" -eq 1 ]; then
  echo "==> [DRY RUN] Would update peglib.h:"
  echo "    CPPPEGLIB_VERSION     = \"$NEW_VERSION\""
  echo "    CPPPEGLIB_VERSION_NUM = \"$VERSION_HEX\""
  echo ""
  echo "==> [DRY RUN] Would commit, tag v$NEW_VERSION, and push."
  echo ""
  echo "==> Dry run complete. No changes were made."
else
  echo "==> Updating peglib.h..."
  sed -i '' "s/#define CPPPEGLIB_VERSION \"[^\"]*\"/#define CPPPEGLIB_VERSION \"$NEW_VERSION\"/" peglib.h
  sed -i '' "s/#define CPPPEGLIB_VERSION_NUM \"0x[0-9a-fA-F]*\"/#define CPPPEGLIB_VERSION_NUM \"$VERSION_HEX\"/" peglib.h
  echo "    CPPPEGLIB_VERSION     = \"$NEW_VERSION\""
  echo "    CPPPEGLIB_VERSION_NUM = \"$VERSION_HEX\""

  # --- Step 6: Commit, tag, and push ---
  echo ""
  echo "==> Committing and tagging..."
  git add peglib.h
  git commit -m "Release v$NEW_VERSION"
  git tag "v$NEW_VERSION"

  echo ""
  echo "==> Pushing..."
  git push && git push --tags

  echo ""
  echo "==> Released v$NEW_VERSION"
fi
