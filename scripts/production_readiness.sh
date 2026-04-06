#!/usr/bin/env bash
set -euo pipefail

# Production readiness orchestrator for the idTech3 engine tree.
# Runs the strongest automated gates available without a GPU: local CI parity,
# full CTest on Vulkan (and OpenGL if built), optional map-load sanity when GAME_BASE is set.
#
# Usage:
#   ./scripts/production_readiness.sh [options]
#
# Options:
#   --skip-opengl     Do not build or test OpenGL (faster)
#   --skip-validate   Skip ./scripts/validate_ci_build.sh (assumes already run)
#
# Environment:
#   GAME_BASE=/abs/path/to/base   If set and non-empty, runs renderer_regression_maps.sh
#                                 after builds (requires full game data + regression maps).
#
# Exit 0 only if every executed step succeeds.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/CMakeLists.txt" ]; then
  PROJECT_ROOT="$SCRIPT_DIR"
elif [ -f "$SCRIPT_DIR/../CMakeLists.txt" ]; then
  PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  echo "Error: Could not find CMakeLists.txt" >&2
  exit 1
fi

SKIP_OPENGL=0
SKIP_VALIDATE=0
for arg in "$@"; do
  case "$arg" in
    --skip-opengl) SKIP_OPENGL=1 ;;
    --skip-validate) SKIP_VALIDATE=1 ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

cd "$PROJECT_ROOT"

echo "=== Production readiness (engine) ==="
echo "Project: $PROJECT_ROOT"
if [ -n "${GAME_BASE:-}" ]; then
  echo "GAME_BASE: $GAME_BASE (map load sanity will run)"
else
  echo "GAME_BASE: (unset — map load sanity skipped)"
fi
echo ""

if [ "$SKIP_VALIDATE" -eq 0 ]; then
  echo "--- 1/4 validate_ci_build (shaders, Vulkan, smoke, renderer regression) ---"
  ./scripts/validate_ci_build.sh
  echo ""
else
  echo "--- 1/4 validate_ci_build SKIPPED ---"
  echo ""
fi

if [ "$SKIP_OPENGL" -eq 0 ]; then
  echo "--- 2/4 OpenGL Release build ---"
  ./scripts/compile_engine.sh opengl
  echo ""
else
  echo "--- 2/4 OpenGL SKIPPED ---"
  echo ""
fi

echo "--- 3/4 CTest (Vulkan build dir) ---"
if [ ! -d "$PROJECT_ROOT/build-vk-Release" ]; then
  echo "Error: build-vk-Release missing; run validate_ci_build or compile_engine.sh vulkan first" >&2
  exit 1
fi
(
  cd "$PROJECT_ROOT/build-vk-Release"
  ctest --output-on-failure
)
echo ""

if [ "$SKIP_OPENGL" -eq 0 ] && [ -d "$PROJECT_ROOT/build-gl-Release" ]; then
  echo "--- 3b/4 CTest (OpenGL build dir) ---"
  (
    cd "$PROJECT_ROOT/build-gl-Release"
    ctest --output-on-failure
  )
  echo ""
fi

if [ -n "${GAME_BASE:-}" ]; then
  echo "--- 4/4 Map load sanity (GAME_BASE) ---"
  GAME_BASE="$GAME_BASE" ./scripts/renderer_regression_maps.sh
  echo ""
else
  echo "--- 4/4 Map load sanity SKIPPED (set GAME_BASE to enable) ---"
  echo ""
fi

echo "=== Production readiness: automated steps complete ==="
echo "Manual: complete Tier C in docs/PRODUCTION_CERTIFICATION.md (GPU proof, validation layers)."
echo "Release: docs/RELEASE_CHECKLIST.md before tagging."
