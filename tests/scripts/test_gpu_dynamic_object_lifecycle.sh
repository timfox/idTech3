#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*"; exit 1; }
grep -q 'vk_gpu_scene_pilot_register_rigid\|r_gpuSceneDynamicPilot' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "dynamic pilot"
grep -q 'vk_gpu_scene_unregister_instance' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "unregister"
echo "PASS: test_gpu_dynamic_object_lifecycle.sh"
