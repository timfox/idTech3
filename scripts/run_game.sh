#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/release"

# Clean shader cache, renderer caches, temporary config data that may stale shaders
rm -rf "$REPO_ROOT/build-vk-Release/shaders" "$REPO_ROOT/.cache" "$REPO_ROOT/cache" 2>/dev/null || true

cd "$BUILD_DIR"
./idtech3 +set r_mode 6 +set fs_game atlas
