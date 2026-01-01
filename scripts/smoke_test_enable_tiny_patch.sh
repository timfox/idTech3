#!/usr/bin/env bash
set -euo pipefail

# Smoke test: enable tiny Vulkan patch and run the engine in Vulkan mode briefly
ROOT="/home/tim/Desktop/idtech3"
LOGS="$ROOT/logs"
RELEASE="$ROOT/release"
BUILD_FLAGS="-set fs_game mymod -set cl_renderer vulkan -set developer 1 -set r_fullscreen 0 -set non_interactive 1"

touch "$LOGS/enable_vulkan_patch1_tiny.flag"

echo "Running smoke test with Vulkan tiny patch enabled..."
echo "Flag: $LOGS/enable_vulkan_patch1_tiny.flag"

cd "$ROOT"
./scripts/compile_engine.sh vulkan >/dev/null
cd "$RELEASE"
./idtech3.x86_64 $BUILD_FLAGS

