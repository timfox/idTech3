#!/usr/bin/env bash
# Foundation Consolidation: frame contract docs + black-frame / contract symbols.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/RENDERER_FRAME_CONTRACT.md"
BF="$ROOT/renderers/vulkan/vk_black_frame.c"
BFH="$ROOT/renderers/vulkan/vk_black_frame.h"

[[ -f "$DOC" ]] || fail "missing $DOC"
for sec in Ownership "Data flow" "Buffer formats" Lifecycle "Fallback behavior" "Debug commands" "Performance cost" "Known limitations" "Next milestone hooks"; do
	grep -q "## $sec" "$DOC" || fail "doc missing section: $sec"
done
pass "RENDERER_FRAME_CONTRACT.md sections present"

grep -q 'renderer_validate_frame' "$BF" || fail "renderer_validate_frame missing in vk_black_frame.c"
grep -q 'renderer_resource_status' "$BF" || fail "renderer_resource_status missing"
grep -q 'renderer_draw_status' "$BF" || fail "renderer_draw_status missing"
grep -q 'gbuffer_bandwidth' "$BF" || fail "gbuffer_bandwidth missing"
pass "black-frame Milestone 1 commands wired"

grep -q 'renderer_frame_status' "$DOC" || fail "doc must mention renderer_frame_status"
grep -q 'renderer_capture_frame_contract' "$DOC" || fail "doc must mention renderer_capture_frame_contract"
grep -q 'SceneHDR' "$DOC" || fail "doc must mention SceneHDR ownership"
pass "frame contract doc symbols present"

if [[ -f "$ROOT/renderers/vulkan/vk_frame_contract.c" ]]; then
	grep -q 'renderer_frame_status' "$ROOT/renderers/vulkan/vk_frame_contract.c" || fail "vk_frame_contract missing renderer_frame_status"
	pass "vk_frame_contract.c present"
else
	grep -q 'vk_frame_contract' "$DOC" || fail "doc must reference vk_frame_contract when source absent"
	pass "vk_frame_contract documented (source optional)"
fi

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All renderer frame contract checks passed."
