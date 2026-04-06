#!/usr/bin/env bash
set -euo pipefail
# Build idtech3_demo.pk3 using the engine's build tree (Vulkan Release by default).

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-vk-Release}"

cmake -S "$ROOT" -B "$BUILD" -DBUILD_EXAMPLE_DEMO_GAME=ON -Wno-dev
cmake --build "$BUILD" --target demo_game_pk3 -j"$(nproc 2>/dev/null || echo 4)"

echo ""
echo "Built: $BUILD/idtech3_demo.pk3"
ls -la "$BUILD/idtech3_demo.pk3"
