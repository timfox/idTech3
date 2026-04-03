#!/usr/bin/env bash
set -euo pipefail

# Headless renderer regression checks (repo + optional game base).
#
# Always: verify regression docs, key generated shader blobs, and GLSL tree.
# Optional: if GAME_BASE is set, require listed BSPs from OPTIONAL_GAME_ASSETS.txt.
#
# Usage:
#   ./scripts/renderer_regression_check.sh
#   GAME_BASE=/path/to/game/base ./scripts/renderer_regression_check.sh
#
# Prerequisites: glslangValidator for GLSL validation (same as smoke_test.sh).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/../CMakeLists.txt" ]; then
  PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  echo "Error: Could not find project root" >&2
  exit 1
fi

cd "$PROJECT_ROOT"

PASS=0
FAIL=0
pass() { PASS=$((PASS + 1)); echo "  ✓ $1"; }
fail() { FAIL=$((FAIL + 1)); echo "  ✗ $1" >&2; }

echo "=== Renderer regression check (headless) ==="
echo ""

echo "Repo manifest (docs + generated shaders):"
MANIFEST="$PROJECT_ROOT/docs/samples/renderer_regression/REGRESSION_REPO_MANIFEST.txt"
if [ ! -f "$MANIFEST" ]; then
  fail "Missing manifest: $MANIFEST"
else
  while IFS= read -r line || [ -n "$line" ]; do
    [[ -z "$line" || "$line" =~ ^# ]] && continue
    p="$PROJECT_ROOT/$line"
    if [ -f "$p" ]; then
      pass "present: $line"
    else
      fail "missing: $line"
    fi
  done < "$MANIFEST"
fi

echo ""
echo "GLSL stage files (glslangValidator -V):"
if command -v glslangValidator &>/dev/null; then
  shader_dir="$PROJECT_ROOT/src/renderers/vulkan/shaders/glsl"
  shader_errors=0
  shader_count=0
  while IFS= read -r -d '' shader; do
    shader_count=$((shader_count + 1))
    rel="${shader#"$PROJECT_ROOT"/}"
    if ! err="$(glslangValidator -V "$shader" -o /dev/null 2>&1)"; then
      echo "$err" >&2
      fail "GLSL: $rel"
      shader_errors=$((shader_errors + 1))
    fi
  done < <(find "$shader_dir" -type f \( -name '*.vert' -o -name '*.frag' -o -name '*.geom' -o -name '*.comp' \) -print0 | sort -z)
  if [ "$shader_count" -eq 0 ]; then
    fail "No GLSL files under $shader_dir"
  elif [ "$shader_errors" -eq 0 ]; then
    pass "all $shader_count GLSL stages validate"
  fi
else
  echo "  ⚠ glslangValidator not found, skipping GLSL validation" >&2
fi

echo ""
if [ -n "${GAME_BASE:-}" ]; then
  echo "Optional game base: $GAME_BASE"
  ASSETS_LIST="$PROJECT_ROOT/docs/samples/renderer_regression/OPTIONAL_GAME_ASSETS.txt"
  req=0
  while IFS= read -r line || [ -n "$line" ]; do
    [[ -z "$line" || "$line" =~ ^# ]] && continue
    req=$((req + 1))
    f="$GAME_BASE/$line"
    if [ -f "$f" ]; then
      pass "game asset: $line"
    else
      fail "missing game asset: $line (expected under GAME_BASE)"
    fi
  done < "$ASSETS_LIST"
  if [ "$req" -eq 0 ]; then
    echo "  (no uncommented paths in OPTIONAL_GAME_ASSETS.txt — add BSP paths to enforce)"
  fi
else
  echo "GAME_BASE unset — skipping optional BSP checks (uncomment paths in OPTIONAL_GAME_ASSETS.txt to enforce in CI)."
fi

echo ""
echo "=== Summary ==="
echo "  Passed: $PASS  Failed: $FAIL"
if [ "$FAIL" -gt 0 ]; then
  echo "RENDERER REGRESSION CHECK FAILED" >&2
  exit 1
fi
echo "RENDERER REGRESSION CHECK PASSED"
exit 0
