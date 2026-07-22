#!/usr/bin/env bash
# Foundation Consolidation: GpuShadowRecord + shadow consumer contract.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/SHADOW_CONTRACT.md"
VSH="$ROOT/renderers/vulkan/vk_vshadow.c"

[[ -f "$DOC" ]] || fail "missing SHADOW_CONTRACT.md"
grep -q 'GpuShadowRecord' "$DOC" || fail "doc must define GpuShadowRecord"
grep -q 'r_shadowConsumerDebug' "$DOC" || fail "doc must mention r_shadowConsumerDebug"
grep -q 'shadow_status' "$DOC" || fail "doc must mention shadow_status"
pass "SHADOW_CONTRACT.md symbols present"

grep -q 'vshadow_status' "$VSH" || fail "vshadow_status missing in vk_vshadow.c"
pass "virtual shadow status command present"

if [[ -f "$ROOT/renderers/vulkan/vk_shadow_contract.h" ]]; then
	grep -q 'GpuShadowRecord' "$ROOT/renderers/vulkan/vk_shadow_contract.h" || fail "GpuShadowRecord type missing"
	grep -q 'shadow_status' "$ROOT/renderers/vulkan/vk_shadow_contract.c" || fail "shadow_status command missing"
	grep -q 'r_shadowConsumerDebug' "$ROOT/renderers/vulkan/vk_shadow_contract.c" || fail "r_shadowConsumerDebug missing"
	pass "vk_shadow_contract module present"
else
	grep -q 'vk_shadow_contract' "$DOC" || fail "doc must reference vk_shadow_contract"
	pass "shadow contract documented (source optional)"
fi

GLSL="$ROOT/renderers/vulkan/shaders/glsl/shadow_contract.glsl"
if [[ -f "$GLSL" ]]; then
	grep -q 'ShadowContract_SampleCSM' "$GLSL" || fail "ShadowContract_SampleCSM missing"
	grep -q 'ShadowContract_SampleCascadeRaw' "$GLSL" || fail "ShadowContract_SampleCascadeRaw missing"
	grep -q 'ShadowContract_SampleCSM_BestFit' "$GLSL" || fail "ShadowContract_SampleCSM_BestFit missing"
	grep -q 'ShadowContract_SampleCSM_FromRecords' "$GLSL" || fail "ShadowContract_SampleCSM_FromRecords missing"
	pass "multi-cascade SampleCSM in shadow_contract.glsl"
fi

grep -q 'shadowCascadeCount' "$ROOT/renderers/vulkan/vk_deferred_gbuffer.c" || fail "deferred push missing shadowCascadeCount"
grep -q 'ShadowContract_SampleCSM' "$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" || fail "deferred lighting must call SampleCSM"
grep -q 'set_layout_oit_shadow\|oit_shadow_descriptor' "$ROOT/renderers/vulkan/vk_shadow_contract.c" || fail "OIT shadow descriptor helpers missing"
grep -q 'ShadowContract_SampleCSM_FromRecords' "$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag" || fail "oit_accum must sample CSM FromRecords"
grep -q 'filterParams\[1\]' "$ROOT/renderers/vulkan/tr_backend.c" || fail "CSM must pack cascade far into filterParams for OIT"
pass "deferred multi-cascade + OIT CSM wiring present"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All shadow consumer parity checks passed."
