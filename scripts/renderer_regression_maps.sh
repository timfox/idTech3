#!/usr/bin/env bash
set -euo pipefail

# Runtime map-load sanity for renderer regression BSPs (dedicated server).
#
# Requires a built idtech3_server and a game tree where fs_basepath/base/ contains
# z_renderer_regression.pk3 (or loose maps/). GAME_BASE must be the **base** directory
# (the folder named `base` that holds pk3s and/or maps/), same as for
# docs/samples/renderer_regression/OPTIONAL_GAME_ASSETS.txt paths.
#
# Usage:
#   GAME_BASE=/abs/path/to/base RELEASE_DIR=/abs/path/to/release ./scripts/renderer_regression_maps.sh
#
# RELEASE_DIR defaults to <repo>/release if unset.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/../CMakeLists.txt" ]; then
  PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  echo "Error: Could not find project root" >&2
  exit 1
fi

RELEASE_DIR="${RELEASE_DIR:-$PROJECT_ROOT/release}"
GAME_BASE="${GAME_BASE:-}"

MAPS=(
  rtest_tangent
  rtest_pbr
  rtest_emissive
  rtest_volumetric
  rtest_postfx
  rtest_parity
)

if [ -z "$GAME_BASE" ] || [ ! -d "$GAME_BASE" ]; then
  echo "Usage: GAME_BASE=/abs/path/to/base [RELEASE_DIR=...] $0" >&2
  echo "  GAME_BASE = game **base** directory (contains *.pk3 and/or maps/)" >&2
  exit 2
fi

INSTALL_ROOT="$(cd "$(dirname "$GAME_BASE")" && pwd)"
BASE_NAME="$(basename "$GAME_BASE")"
SERVER="$RELEASE_DIR/idtech3_server"

if [ ! -x "$SERVER" ] && [ ! -f "$SERVER" ]; then
  echo "Error: idtech3_server not found at $SERVER (set RELEASE_DIR?)" >&2
  exit 2
fi

# Patterns indicating map load / server init failed (grep -Ei across log).
FAIL_PATTERN='ERROR:|Server fatal crashed|couldn'\''t load|could not load|CM_LoadMap:|SIGSEGV|segfault|core dump|\babort\b'

PASS=0
FAIL=0
pass() { PASS=$((PASS + 1)); echo "  ✓ $1"; }
fail() { FAIL=$((FAIL + 1)); echo "  ✗ $1" >&2; }

echo "=== Renderer regression map load (dedicated) ==="
echo "  Server:  $SERVER"
echo "  fs_basepath -> $INSTALL_ROOT (game dir: $BASE_NAME)"
echo ""

for map in "${MAPS[@]}"; do
  # shellcheck disable=SC2086
  out="$(timeout 45 "$SERVER" \
    +set dedicated 1 \
    +set fs_basepath "$INSTALL_ROOT" \
    +set fs_game "$BASE_NAME" \
    +set com_hunkMegs 128 \
    +map "$map" \
    +quit 2>&1 || true)"

  if echo "$out" | grep -Eiq "$FAIL_PATTERN"; then
    echo "$out" | grep -Ei "$FAIL_PATTERN" | head -20 >&2
    fail "map $map: error in server log"
    continue
  fi

  if ! echo "$out" | grep -q "Server: $map"; then
    fail "map $map: missing \"Server: $map\" in log (map may not have started)"
    continue
  fi

  pass "map $map loaded (no error pattern in log)"
done

echo ""
echo "=== Summary === Passed: $PASS  Failed: $FAIL"
if [ "$FAIL" -gt 0 ]; then
  echo "MAP LOAD SANITY FAILED" >&2
  exit 1
fi
echo "MAP LOAD SANITY PASSED"
exit 0
