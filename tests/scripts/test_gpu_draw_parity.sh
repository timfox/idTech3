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

grep -q 'r_gpuDriven' "$SRC" || fail "r_gpuDriven missing"
grep -q '0", "2"\|"0", "2"' "$SRC" || grep -q 'r_gpuDriven.*0.*2' "$SRC" || fail "r_gpuDriven range 0-2"
grep -q 'r_gpuDrawForceStaleCommand\|s_drawCountPublished' "$SRC" || fail "stale prevention"
pass "GPU cull/indirect + stale prevention present"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All GPU draw parity checks passed."
