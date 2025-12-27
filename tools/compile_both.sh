#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <modname>"
    exit 1
fi

MODNAME="$1"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

# Compile engine in Release mode, cleaning first
"$ROOT_DIR"/tools/compile_engine.sh clean Release

# Compile game VM for the specified mod, cleaning first
"$ROOT_DIR"/tools/compile_game.sh clean "$MODNAME"

# Ensure loose autoexec.cfg is available (engine may not exec from PK3)
if [ -f "$ROOT_DIR/$MODNAME/autoexec.cfg" ]; then
  cp "$ROOT_DIR/$MODNAME/autoexec.cfg" "$ROOT_DIR/release/$MODNAME/autoexec.cfg"
fi

# Run the game with explicit paths so the mod assets/autoexec are found
"$ROOT_DIR"/release/idtech3.x86_64 \
  +set fs_basepath "$ROOT_DIR/release" \
  +set fs_homepath "$ROOT_DIR/release" \
  +set fs_game "$MODNAME"

# Coverage build/report (Debug + ENABLE_COVERAGE=ON) if gcovr is available
if command -v gcovr >/dev/null 2>&1; then
  echo "[compile_both] Running coverage build..."
  cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build-coverage" -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
  cmake --build "$ROOT_DIR/build-coverage" --target coverage
  echo "[compile_both] Coverage artifacts should be in build-coverage/"
else
  echo "[compile_both] gcovr not found; skipping coverage build."
fi