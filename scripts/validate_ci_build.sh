#!/usr/bin/env bash
set -euo pipefail

# Local CI validation: shader compilation, Vulkan build, smoke test.
# Mirrors the Ubuntu x86_64 CI pipeline for pre-push validation.
#
# Usage: ./scripts/validate_ci_build.sh
# Prerequisites: glslang-tools, python3, build deps (see docs/DEVELOPMENT_SETUP.md)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/CMakeLists.txt" ]; then
  PROJECT_ROOT="$SCRIPT_DIR"
elif [ -f "$SCRIPT_DIR/../CMakeLists.txt" ]; then
  PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  echo "Error: Could not find CMakeLists.txt" >&2
  exit 1
fi

cd "$PROJECT_ROOT"

echo "=== CI Validation (local) ==="
echo ""

echo "1. Verifying shader compilation..."
./scripts/compile_shaders.sh
echo ""

echo "2. Building Vulkan (Release)..."
./scripts/compile_engine.sh vulkan
echo ""

echo "3. Running smoke test (includes Q3/OA compat check)..."
./scripts/smoke_test.sh release
echo ""

echo "4. CTest (mirrors ubuntu-x86_64 job: smoke + renderer + scripts + units)..."
if [ -d build-vk-Release ]; then
  ( cd build-vk-Release && ctest -C Release --output-on-failure )
else
  echo "Warning: build-vk-Release missing; skip ctest (run compile_engine.sh vulkan first)" >&2
fi
echo ""

echo "5. Optional submodule init script (dry-run)..."
./scripts/init_optional_submodules.sh --all --dry-run
echo ""

echo "6. Renderer regression check (repo + GLSL)..."
./scripts/renderer_regression_check.sh
echo ""

DEVDATA_BASE="$PROJECT_ROOT/docs/renderer_validation/devdata/rtest_base"
if [ -f "$DEVDATA_BASE/vm/qagame.qvm" ]; then
  echo "6b. Tier B devdata map load (dedicated, no retail pk3)..."
  chmod +x ./scripts/run_renderer_tier_b_devdata.sh
  ./scripts/run_renderer_tier_b_devdata.sh
  echo ""
else
  echo "6b. Tier B devdata skipped (run ./scripts/build_renderer_devdata.sh to enable)"
  echo ""
fi
echo ""

echo "7. Demo mod pack layout (idtech3_demo.pk3)..."
chmod +x ./tests/scripts/test_demo_game_pk3.sh
./tests/scripts/test_demo_game_pk3.sh
echo ""

echo "=== CI validation passed ==="
