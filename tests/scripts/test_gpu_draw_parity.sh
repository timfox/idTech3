#!/usr/bin/env bash
# Foundation Consolidation: GPU-driven cull/indirect + draw compare symbols.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/GPU_DRIVEN_RENDERING.md"
SRC="$ROOT/renderers/vulkan/vk_gpu_scene.c"

[[ -f "$DOC" ]] || fail "missing GPU_DRIVEN_RENDERING.md"
grep -q 'r_gpuDriven' "$DOC" || fail "doc must mention r_gpuDriven"
grep -q 'r_gpuDrawCompare' "$DOC" || fail "doc must mention r_gpuDrawCompare"
grep -q 'PVS' "$DOC" || fail "doc must mention PVS stage"
grep -qi 'stale' "$DOC" || fail "doc must mention stale indirect clearing"
pass "GPU_DRIVEN_RENDERING.md symbols present"

grep -q 'r_gpuScene' "$SRC" || fail "r_gpuScene missing"
grep -q 'r_gpuSceneCull' "$SRC" || fail "r_gpuSceneCull missing"
grep -q 'r_gpuSceneIndirect' "$SRC" || fail "r_gpuSceneIndirect missing"
grep -q 'vk_gpu_scene_cull_and_build_indirect' "$SRC" || fail "cull/indirect entry missing"
grep -q 's_rejectFrustum\|s_rejectHiz\|s_rejectLod' "$SRC" || fail "reject telemetry missing"
pass "GPU cull/indirect implementation present"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All GPU draw parity checks passed."
