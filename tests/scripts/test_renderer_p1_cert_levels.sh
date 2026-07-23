#!/usr/bin/env bash
# Static gate: Renderer P1 honest multi-level certification (Phase 1.5).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "PASS: $*"; }

H="$ROOT/renderers/vulkan/vk_renderer_iq_p1.h"
C="$ROOT/renderers/vulkan/vk_renderer_p1_cert.c"
DOC="$ROOT/docs/RENDERER_P1_CERTIFICATION.md"

[[ -f "$H" ]] || fail "missing $H"
[[ -f "$C" ]] || fail "missing $C"
[[ -f "$DOC" ]] || fail "missing $DOC"

for lvl in RENDERER_P1_UNCERTIFIED RENDERER_P1_STATIC_READY RENDERER_P1_PROFILE_CERTIFIED \
	RENDERER_P1_GPU_CORE_CERTIFIED RENDERER_P1_TEMPORAL_CERTIFIED RENDERER_P1_EDGE_CERTIFIED \
	RENDERER_P1_LIGHTING_PARITY_CERTIFIED RENDERER_P1_IMAGE_QUALITY_CERTIFIED; do
	grep -q "$lvl" "$H" || fail "enum missing $lvl"
done
pass "level enum"

grep -q 'P1_EVIDENCE_GPU_READBACK' "$H" || fail 'GPU_READBACK evidence missing'
grep -q 'PROFILE_CERTIFIED' "$C" || fail 'cert module missing PROFILE'
grep -qi 'MANUAL_OVERRIDE never grants IMAGE_QUALITY' "$C" || \
	grep -q 'cannot grant IMAGE_QUALITY' "$C" || fail 'manual override blocked for final'
grep -qi 'maximum from static/cvar\|PROFILE alone\|PROFILE_CERTIFIED is the maximum' "$DOC" || \
	grep -qi 'cvars alone' "$DOC" || fail 'doc must state profile alone is not IMAGE_QUALITY'

# Final label must not be granted by profile validate alone in status path.
if grep -n 'IMAGE_QUALITY_CERTIFIED' "$ROOT/renderers/vulkan/vk_renderer_iq_p1.c" | grep -q 'fail == 0'; then
	fail 'p1 status still grants IMAGE_QUALITY from gate fail==0'
fi
grep -q 'vk_renderer_p1_cert_level\|vk_renderer_p1_level_name' "$ROOT/renderers/vulkan/vk_renderer_iq_p1.c" || \
	fail 'status must use cert ladder level'
pass 'honest promotion rules'

[[ $failures -eq 0 ]] || { echo "$failures failed"; exit 1; }
echo "All renderer_p1_cert_levels checks passed."
