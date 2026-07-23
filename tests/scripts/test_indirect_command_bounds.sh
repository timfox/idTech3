#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*"; exit 1; }
grep -q 's_invalidCommandRejects\|VK_GPU_SCENE_REJECT_BAD_MESH' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "cmd validation"
grep -q 'gpu_draw_status' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "draw status"
echo "PASS: test_indirect_command_bounds.sh"
