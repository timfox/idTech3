#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*"; exit 1; }
grep -q 's_fallbackObjects\|r_gpuSceneMaxObjects\|capacity exceeded' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "capacity"
echo "PASS: test_gpu_capacity_fallback.sh"
