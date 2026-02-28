#!/usr/bin/env bash
# Run clang-tidy on the id Tech 3 engine source.
# Requires: clang-tidy, cmake build with CMAKE_EXPORT_COMPILE_COMMANDS=ON
#
# Usage: ./scripts/run_clang_tidy.sh [--fix] [path...]
#   --fix    Apply suggested fixes where safe
#   path     Limit analysis to specific files/directories (default: src/)

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

BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build-vk-Release}"
COMPILE_DB="$BUILD_DIR/compile_commands.json"
FIX=""
PATHS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --fix)
      FIX="--fix"
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

if ! command -v clang-tidy &>/dev/null; then
  echo "Error: clang-tidy not found. Install it (e.g. apt install clang-tidy)" >&2
  exit 1
fi

if [ ! -f "$COMPILE_DB" ]; then
  echo "Generating compile_commands.json..."
  mkdir -p "$BUILD_DIR"
  cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DUSE_VULKAN=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -Wno-dev
fi

# Find C/C++ source files in paths (exclude external)
SOURCES=()
for p in "${PATHS[@]}"; do
  while IFS= read -r -d '' f; do
    [[ "$f" == *"/external/"* ]] && continue
    SOURCES+=("$f")
  done < <(find "$p" -type f \( -name '*.c' -o -name '*.cpp' \) -print0 2>/dev/null)
done

if [ ${#SOURCES[@]} -eq 0 ]; then
  echo "No source files found."
  exit 0
fi

echo "Running clang-tidy on ${#SOURCES[@]} files..."
for f in "${SOURCES[@]}"; do
  clang-tidy -p "$BUILD_DIR" $FIX "$f" \
    -header-filter='src/.*' \
    2>/dev/null || true
done

echo "clang-tidy done."
