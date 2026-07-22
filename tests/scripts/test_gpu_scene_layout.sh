#!/usr/bin/env bash
# Foundation Consolidation: GPU scene schema + layout symbols.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/GPU_SCENE.md"
HDR="$ROOT/renderers/vulkan/vk_gpu_scene.h"
SRC="$ROOT/renderers/vulkan/vk_gpu_scene.c"

[[ -f "$DOC" ]] || fail "missing GPU_SCENE.md"
for sec in Ownership "Data flow" "Buffer formats" Lifecycle "Fallback behavior" "Debug commands" "Performance cost" "Known limitations" "Next milestone hooks"; do
	grep -q "## $sec" "$DOC" || fail "doc missing section: $sec"
done
pass "GPU_SCENE.md sections present"

grep -q 'GpuSceneObject' "$DOC" || fail "doc must define GpuSceneObject"
grep -q 'gpu_scene_layout' "$DOC" || fail "doc must mention gpu_scene_layout"
grep -q 'r_gpuSceneDebug' "$DOC" || fail "doc must mention r_gpuSceneDebug"
pass "GPU scene doc symbols present"

[[ -f "$HDR" ]] || fail "missing vk_gpu_scene.h"
grep -q 'vkGpuSceneInstance_t' "$HDR" || fail "vkGpuSceneInstance_t missing"
grep -q 'vkGpuSceneMesh_t' "$HDR" || fail "vkGpuSceneMesh_t missing"
grep -q 'VK_GPU_SCENE_MAX_INSTANCES' "$HDR" || fail "capacity constants missing"
pass "vk_gpu_scene.h schema present"

grep -q 'gpu_scene_status' "$SRC" || fail "gpu_scene_status command missing"
grep -q 'r_gpuSceneDebug' "$SRC" || fail "r_gpuSceneDebug cvar missing"
grep -q 'r_gpuScene' "$SRC" || fail "r_gpuScene cvar missing"
pass "vk_gpu_scene.c wiring present"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All GPU scene layout checks passed."
