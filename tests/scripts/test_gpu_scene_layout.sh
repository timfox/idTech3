#!/usr/bin/env bash
# Foundation Consolidation: GPU scene schema + layout symbols (M1 Phase 1).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/GPU_SCENE.md"
HDR="$ROOT/renderers/vulkan/vk_gpu_scene.h"
SRC="$ROOT/renderers/vulkan/vk_gpu_scene.c"

[[ -f "$DOC" ]] || fail "missing GPU_SCENE.md"
grep -q '## Ownership' "$DOC" || fail "doc missing Ownership"
grep -q 'gpuSceneObject_t' "$DOC" || fail "doc must define gpuSceneObject_t"
grep -q 'Shared consumers\|Deferred opaque\|Depth prepass\|temporal velocity' "$DOC" || fail "doc must list shared consumers"
grep -q 'gpu_scene_layout' "$DOC" || fail "doc must mention gpu_scene_layout"
grep -q 'r_gpuSceneDebug' "$DOC" || fail "doc must mention r_gpuSceneDebug"
grep -q 'object SSBO\|persistent object' "$DOC" || fail "doc must mention object SSBO"
pass "GPU scene doc symbols present"

[[ -f "$HDR" ]] || fail "missing vk_gpu_scene.h"
grep -q 'typedef struct gpuSceneObject_s' "$HDR" || fail "gpuSceneObject_s missing"
grep -q 'gpuSceneObject_t' "$HDR" || fail "gpuSceneObject_t missing"
grep -q 'currentModel\|previousModel\|boundsSphere\|objectGeneration\|instanceDataIndex' "$HDR" || fail "preferred contract fields"
grep -q 'VK_GPU_PATH_DEPTH_PREPASS\|VK_GPU_PATH_VELOCITY\|VK_GPU_PATH_OBJECT_ID' "$HDR" || fail "shared path enums"
grep -q 'vkGpuSceneMesh_t' "$HDR" || fail "vkGpuSceneMesh_t missing"
grep -q 'VK_GPU_SCENE_MAX_INSTANCES' "$HDR" || fail "capacity constants missing"
pass "vk_gpu_scene.h schema present"

grep -q 'gpu_scene_layout' "$SRC" || fail "gpu_scene_layout command missing"
grep -q 'gpu_scene_status' "$SRC" || fail "gpu_scene_status command missing"
grep -q 'r_gpuSceneDebug' "$SRC" || fail "r_gpuSceneDebug cvar missing"
grep -q 'r_gpuScene' "$SRC" || fail "r_gpuScene cvar missing"
grep -q 'GPUScene_UploadObjects\|s_objectBuf\|object_buffer_ready' "$SRC" || fail "persistent object SSBO wiring"
grep -q 'objectGeneration\|currentModel\|previousModel' "$HDR" || fail "M1 identity/transform fields"
pass "vk_gpu_scene.c wiring present"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All GPU scene layout checks passed."
