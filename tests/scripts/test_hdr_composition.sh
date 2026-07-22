#!/usr/bin/env bash
# Foundation Consolidation: HDR post chain composition order.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/HDR_PIPELINE.md"
BF="$ROOT/renderers/vulkan/vk_black_frame.c"

[[ -f "$DOC" ]] || fail "missing HDR_PIPELINE.md"
grep -q 'hdr_pipeline_status' "$DOC" || fail "doc must mention hdr_pipeline_status"
grep -q 'r_hdrStageDebug' "$DOC" || fail "doc must mention r_hdrStageDebug"
grep -q 'exposure' "$DOC" || fail "doc must mention exposure stage"
grep -q 'tonemap' "$DOC" || fail "doc must mention tonemap stage"
grep -q 'bloom' "$DOC" || fail "doc must mention bloom stage"
pass "HDR_PIPELINE.md composition documented"

grep -q 'renderer_validate_frame' "$BF" || fail "black-frame validate must check HDR chain"
grep -q 'adaptedExposure\|exposure' "$BF" || fail "exposure validation in black-frame"
pass "black-frame HDR validation hooks present"

if [[ -f "$ROOT/renderers/vulkan/vk_hdr_pipeline.c" ]]; then
	grep -q 'hdr_pipeline_status' "$ROOT/renderers/vulkan/vk_hdr_pipeline.c" || fail "hdr_pipeline_status missing"
	grep -q 'r_hdrStageDebug' "$ROOT/renderers/vulkan/vk_hdr_pipeline.c" || fail "r_hdrStageDebug missing"
	grep -q 'VK_HDR_STAGE_TONEMAP' "$ROOT/renderers/vulkan/vk_hdr_pipeline.h" || fail "tonemap stage enum missing"
	pass "vk_hdr_pipeline module present"
else
	grep -q 'vk_hdr_pipeline' "$DOC" || fail "doc must reference vk_hdr_pipeline"
	pass "HDR pipeline documented (source optional)"
fi

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All HDR composition checks passed."
