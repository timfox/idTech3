#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*"; exit 1; }
grep -q 'gpu_scene_object_status' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "object status"
grep -q 'VK_GPU_INVALIDATE_' "$ROOT/renderers/vulkan/vk_gpu_scene.h" || fail "invalidate reasons"
grep -q 'GPU Object Identity\|objectGeneration' "$ROOT/docs/GPU_OBJECT_IDENTITY.md" || fail "doc"
echo "PASS: test_gpu_object_identity.sh"
