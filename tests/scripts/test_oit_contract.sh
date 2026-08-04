#!/usr/bin/env bash
# Color Pipeline Phase 2.1: frozen production WBOIT contract.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/WBOIT_CONTRACT.md"
H="$ROOT/renderers/vulkan/vk_oit_contract.h"
C="$ROOT/renderers/vulkan/vk_oit_contract.c"
ACCUM="$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag"
RESOLVE="$ROOT/renderers/vulkan/shaders/glsl/oit_resolve.frag"
PIPE="$ROOT/renderers/vulkan/vk_pipeline_helpers.c"
RP="$ROOT/renderers/vulkan/vk_render_pass.c"
ROUTE="$ROOT/renderers/vulkan/vk_transparency_route.c"

[[ -f "$DOC" ]] || fail "missing WBOIT_CONTRACT.md"
[[ -f "$H" ]] || fail "missing vk_oit_contract.h"
[[ -f "$C" ]] || fail "missing vk_oit_contract.c"

grep -q 'oitContract_t' "$H" || fail "oitContract_t missing"
grep -q 'OIT_CONTRACT_VERSION' "$H" || fail "OIT_CONTRACT_VERSION missing"
grep -q 'contractHash' "$H" || fail "contractHash missing"
grep -q 'sceneLinear' "$H" || fail "sceneLinear missing"
grep -q 'preExposed' "$H" || fail "preExposed missing"
grep -q 'premultipliedRadiance' "$H" || fail "premultipliedRadiance missing"
grep -q 'fogAppliedPerFragment' "$H" || fail "fogAppliedPerFragment missing"
grep -q 'accumClear' "$H" || fail "accumClear missing"
grep -q 'revealageClear' "$H" || fail "revealageClear missing"
pass "oitContract_t surface"

grep -q 'oit_contract_status' "$C" || fail "oit_contract_status missing"
grep -q 'oit_contract_validate' "$C" || fail "oit_contract_validate missing"
grep -q 'vk_oit_contract_register' "$C" || fail "register missing"
grep -q 'VK_FORMAT_R16G16B16A16_SFLOAT' "$C" || fail "accum format missing"
grep -q 'VK_FORMAT_R16_SFLOAT' "$C" || fail "reveal format missing"
grep -q 'VK_BLEND_FACTOR_ONE' "$C" || fail "accum blend ONE missing"
grep -q 'VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR' "$C" || fail "reveal blend missing"
grep -q 'revealageClear = 1.0f' "$C" || fail "reveal clear 1.0 missing"
pass "vk_oit_contract implementation"

grep -q 'R16G16B16A16_SFLOAT' "$DOC" || fail "doc accum format"
grep -q 'product(1' "$DOC" || fail "doc revealage product"
grep -q 'C_avg' "$DOC" || fail "doc resolve equation"
grep -q 'preExposed' "$DOC" || fail "doc pre-exposure"
grep -q 'oit_contract_status' "$DOC" || fail "doc status command"
pass "WBOIT_CONTRACT.md"

# Implementation must match freeze (shaders / blend / clear).
grep -q 'out_color = vec4( litRgb \* alpha, alpha ) \* w' "$ACCUM" || fail "accum equation drift"
grep -q 'OitWeight_BoundedProduction\|oit_weight.glsl' "$ACCUM" || fail "bounded weight helper missing"
grep -q 'out_reveal = alpha' "$ACCUM" || fail "reveal shader out=alpha drift"
grep -q 'C_avg \* coverage + opaque \* revealage\|c_avg \* coverage + opaque \* revealage' "$RESOLVE" || \
	grep -q 'c_avg \* coverage + opaque \* revealage' "$RESOLVE" || fail "resolve equation drift"
grep -q 'accum.a < 1e-5' "$RESOLVE" || fail "empty-pixel preserve missing"
grep -q 'pc.bucket == 1' "$RESOLVE" || fail "additive bucket must bypass revealage coverage"
grep -q 'push_data\[3\] = ( bucket == 1 )' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || fail "resolve must identify additive bucket"
if grep -A6 's_reads_oit_resolve' "$ROOT/renderers/vulkan/vk_pass_registry.c" | grep -q 'VK_SPINE_RES_OIT_MOMENTS'; then
	fail "WBOIT resolve registry must not require MBOIT moments"
fi
grep -q 'mboit_moments' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || fail "MBOIT resolve must explicitly own moments layout"
grep -q 'dstColorBlendFactor = VK_BLEND_FACTOR_ONE' "$PIPE" || fail "accum ONE/ONE drift"
grep -q 'ONE_MINUS_SRC_COLOR' "$PIPE" || fail "reveal blend drift"
grep -A6 'renderPass == vk.render_pass.oit_accum' "$RP" | grep -q '1.0f' || fail "reveal clear 1.0 in render pass"
pass "implementation matches freeze"

grep -q 'vk_oit_contract_register' "$ROUTE" || fail "transparency route must register contract"
grep -q 'oit_contract_status' "$ROUTE" || fail "oit_status must reference contract"
pass "wiring"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All OIT contract freeze checks passed."
