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
# Optional extra BSP names (space-separated), e.g. custom Tier B maps with mixed dlights:
#   MAPS_EXTRA="rtest_mixed_dlights" GAME_BASE=... ./scripts/renderer_regression_maps.sh
# CI: set repository variable IDTECH3_MAPS_EXTRA (see docs/renderer_validation/SELF_HOSTED_TIER_B.md).
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
# qagame.qvm may print "Couldn't load symbol file: vm/qagame.map" when debug
# symbols are absent; that is harmless and must not fail renderer map smoke.
FAIL_PATTERN='ERROR:|Server fatal crashed|could not load|CM_LoadMap:|SIGSEGV|segfault|core dump|\babort\b'
IGNORED_FAIL_PATTERN='Couldn'\''t load symbol file:'

PASS=0
FAIL=0
pass() { PASS=$((PASS + 1)); echo "  ✓ $1"; }
fail() { FAIL=$((FAIL + 1)); echo "  ✗ $1" >&2; }

echo "=== Renderer regression map load (dedicated) ==="
echo "  Server:  $SERVER"
echo "  fs_basepath -> $INSTALL_ROOT (game dir: $BASE_NAME)"
echo ""

run_map_check() {
  local map="$1"
  # shellcheck disable=SC2086
  out="$(timeout 45 "$SERVER" \
    +set dedicated 1 \
    +set fs_basepath "$INSTALL_ROOT" \
    +set fs_game "$BASE_NAME" \
    +set vm_game 2 \
    +set bot_enable 0 \
    +set com_hunkMegs 128 \
    +map "$map" \
    +quit 2>&1 || true)"

  fail_lines="$(echo "$out" | grep -Ei "$FAIL_PATTERN" | grep -Eiv "$IGNORED_FAIL_PATTERN" || true)"
  if [ -n "$fail_lines" ]; then
    echo "$fail_lines" | head -20 >&2
    fail "map $map: error in server log"
    return
  fi

  if ! echo "$out" | grep -q "Server: $map"; then
    fail "map $map: missing \"Server: $map\" in log (map may not have started)"
    return
  fi

  pass "map $map loaded (no error pattern in log)"
}

for map in "${MAPS[@]}"; do
  run_map_check "$map"
done

if [ -n "${MAPS_EXTRA:-}" ]; then
  echo ""
  echo "MAPS_EXTRA (optional additional BSP names): ${MAPS_EXTRA}"
  # shellcheck disable=SC2086
  for map in ${MAPS_EXTRA}; do
    [ -z "$map" ] && continue
    run_map_check "$map"
  done
fi

echo ""
echo "=== Summary === Passed: $PASS  Failed: $FAIL"
if [ "$FAIL" -gt 0 ]; then
  echo "MAP LOAD SANITY FAILED" >&2
  exit 1
fi
echo "MAP LOAD SANITY PASSED"
exit 0
