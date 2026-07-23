#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*"; exit 1; }
grep -q 'GpuFrustum_SphereVisible\|vk_gpu_frustum_sphere_visible' "$ROOT/renderers/vulkan/vk_gpu_frustum_math.h" "$ROOT/renderers/vulkan/vk_gpu_visibility.c" || fail "frustum math"
grep -q 'VISIBILITY_FRUSTUM' "$ROOT/renderers/vulkan/vk_gpu_visibility.h" || fail "stage"
echo "PASS: test_gpu_frustum_culling.sh"
