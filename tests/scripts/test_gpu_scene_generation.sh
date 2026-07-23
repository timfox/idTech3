#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*"; exit 1; }
grep -q 's_generation\|objectGeneration' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "generation"
grep -q 's_freeList\|unregister_instance' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "free list reuse"
echo "PASS: test_gpu_scene_generation.sh"
