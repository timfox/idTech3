#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*"; exit 1; }
grep -q 'r_gpuOcclusion' "$ROOT/renderers/vulkan/vk_gpu_visibility.c" || fail "occlusion cvar"
grep -q 'vk_gpu_occlusion_enabled' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "occlusion gate"
echo "PASS: test_gpu_occlusion_culling.sh"
