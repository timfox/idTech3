#!/usr/bin/env bash
# Run cppcheck static analysis on the id Tech 3 engine source.
# Requires: cppcheck
#
# Usage: ./scripts/run_cppcheck.sh [--enable-all] [path...]
#   --enable-all   Enable all checks (slower, more thorough)
#   path          Limit analysis to specific files/directories (default: src/)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/CMakeLists.txt" ]; then
  PROJECT_ROOT="$SCRIPT_DIR"
elif [ -f "$SCRIPT_DIR/../CMakeLists.txt" ]; then
  PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  echo "Error: Could not find CMakeLists.txt" >&2
  exit 1
fi

ENABLE="warning,style,performance,portability"
PATHS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --enable-all)
      ENABLE="all"
      shift
      ;;
    *)
      PATHS+=("$1")
      shift
      ;;
  esac
done

if [ ${#PATHS[@]} -eq 0 ]; then
  PATHS=("$PROJECT_ROOT/src")
fi

if ! command -v cppcheck &>/dev/null; then
  echo "Error: cppcheck not found. Install it (e.g. apt install cppcheck)" >&2
  exit 1
fi

echo "Running cppcheck (enable=$ENABLE) on ${PATHS[*]}..."
cppcheck \
  --enable="$ENABLE" \
  --suppress=missingIncludeSystem \
  --suppress=unmatchedSuppression \
  --inline-suppr \
  -I"$PROJECT_ROOT/src" \
  -I"$PROJECT_ROOT/src/renderers/common" \
  -DUSE_VULKAN=1 \
  -q \
  "${PATHS[@]}" 2>&1 || true

echo "cppcheck done."
