#!/usr/bin/env bash
# Color Pipeline Phase 2.3.2: certified positive view-depth for WBOIT fog/weight.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

GLSL="$ROOT/renderers/vulkan/shaders/glsl/depth_view.glsl"
OIT="$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag"
MBOIT="$ROOT/renderers/vulkan/shaders/glsl/oit_accum_mboit.frag"
FP="$ROOT/renderers/vulkan/vk_forward_plus.c"
CERT="$ROOT/renderers/vulkan/vk_oit_certify.c"
DOC="$ROOT/docs/DEPTH_CONTRACT.md"
FOG_DOC="$ROOT/docs/WBOIT_FOG_LAYERS.md"
DEPTH_C="$ROOT/renderers/vulkan/vk_depth_contract.c"

[[ -f "$GLSL" ]] || fail "missing depth_view.glsl"
[[ -f "$OIT" ]] || fail "missing oit_accum.frag"

grep -q 'Depth_LinearizeReversedZ' "$GLSL" || fail "linearize helper missing"
grep -q 'Depth_PositiveViewFromWorld' "$GLSL" || fail "world view-depth helper missing"
grep -q 'Depth_ViewDepthToTraditional01' "$GLSL" || fail "traditional01 helper missing"
pass "depth_view.glsl helpers"

grep -q 'depth_view.glsl' "$OIT" || fail "oit_accum must include depth_view.glsl"
grep -q 'fp_view_forward' "$OIT" || fail "oit_accum must declare fp_view_forward"
grep -q 'Depth_PositiveViewFromWorld' "$OIT" || fail "oit_accum must use certified view-depth"
grep -q 'Depth_ViewDepthToTraditional01' "$OIT" || fail "oit_accum weight must use traditional01 from view-depth"
# Production fog must not use bare Euclidean length against fp_view_org.
if grep -n 'length( *frag_world_pos *- *fp_params\.fp_view_org' "$OIT" | grep -v 'Depth_CameraDistance\|fogDebug == 8\|cert'; then
	fail "oit_accum still uses Euclidean camera distance for production fog/weight"
else
	pass "oit_accum has no production camera-distance fog"
fi

grep -q 'depth_view.glsl' "$MBOIT" || fail "mboit must include depth_view.glsl"
grep -q 'Depth_PositiveViewFromWorld' "$MBOIT" || fail "mboit must use certified view-depth"
pass "oit_accum_mboit migrated"

grep -q 'param_f\[32\]' "$FP" || fail "forward+ must upload fp_view_forward at floats 32-35"
grep -q 'axis\[0\]' "$FP" || fail "forward+ must use viewParms.or.axis[0]"
pass "forward+ uploads view forward"

grep -q 'certified positive view-depth' "$CERT" || fail "oit_fog_status must report certified view-depth"
grep -q 'vk_depth_positive_view_from_world\|Depth_PositiveViewFromWorld\|migrated' "$DEPTH_C" || \
	fail "depth contract print should note migration/helpers"
pass "status strings"

grep -q 'Depth_PositiveViewFromWorld\|Phase 2.3.2\|fp_view_forward' "$DOC" || fail "DEPTH_CONTRACT.md must document 2.3.2"
grep -q 'Depth_PositiveViewFromWorld\|certified positive view-depth\|depth_view.glsl' "$FOG_DOC" || \
	fail "WBOIT_FOG_LAYERS.md must document certified view-depth"
pass "docs"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All OIT view-depth migration checks passed."
