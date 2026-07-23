#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*"; exit 1; }
grep -q 'VK_GPU_DRAW_LIST_' "$ROOT/renderers/vulkan/vk_gpu_visibility.h" || fail "lists"
grep -q 'VK_GPU_DRAW_LIST_DEPTH_PREPASS\|VK_GPU_DRAW_LIST_VELOCITY' "$ROOT/renderers/vulkan/vk_gpu_visibility.h" || fail "depth/velocity lists"
grep -q 's_listCounts' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "bucket counts"
grep -q 'VK_GPU_PATH_DEPTH_PREPASS\|VK_GPU_PATH_VELOCITY' "$ROOT/renderers/vulkan/vk_gpu_scene.h" || fail "path enums"
echo "PASS: test_gpu_render_path_buckets.sh"
