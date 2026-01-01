#!/usr/bin/env bash
set -euo pipefail

# Smoke test: Force GL baseline to establish stability before Vulkan testing
ROOT="/home/tim/Desktop/idtech3"
LOGS="$ROOT/logs"
RELEASE="$ROOT/release"

echo "Running GL baseline smoke test (safe_mode)..."
touch "$LOGS/safe_mode.flag"
echo "Safe mode flag created at $LOGS/safe_mode.flag"

echo "Building Vulkan backend (for completeness; runtime will fallback to GL)"
./scripts/compile_engine.sh vulkan >/dev/null

BUILD_FLAGS="-set fs_game mymod -set cl_renderer opengl -set developer 1 -set r_fullscreen 0 -set non_interactive 1"
cd "$RELEASE"
./idtech3.x86_64 $BUILD_FLAGS

