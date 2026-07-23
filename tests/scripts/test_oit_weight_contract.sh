#!/usr/bin/env bash
# Color Pipeline Phase 2.5: bounded WBOIT weight + additive separation + blend routing.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/WBOIT_WEIGHT_CONTRACT.md"
H="$ROOT/renderers/vulkan/vk_oit_weight_contract.h"
C="$ROOT/renderers/vulkan/vk_oit_weight_contract.c"
GLSL="$ROOT/renderers/vulkan/shaders/glsl/oit_weight.glsl"
ACCUM="$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag"
OIT_H="$ROOT/renderers/vulkan/vk_oit_contract.h"
INIT="$ROOT/renderers/vulkan/tr_init.c"
BACK="$ROOT/renderers/vulkan/tr_backend.c"
ROUTE="$ROOT/renderers/vulkan/vk_transparency_route.c"
PIPE="$ROOT/renderers/vulkan/vk_pipeline_helpers.c"

[[ -f "$DOC" ]] || fail "missing WBOIT_WEIGHT_CONTRACT.md"
[[ -f "$H" ]] || fail "missing vk_oit_weight_contract.h"
[[ -f "$GLSL" ]] || fail "missing oit_weight.glsl"

grep -q 'oitWeightContract_t' "$H" || fail "oitWeightContract_t missing"
grep -q 'OIT_WEIGHT_BOUNDED_PRODUCTION' "$H" || fail "BOUNDED_PRODUCTION missing"
grep -q 'OIT_WEIGHT_ALPHA_REFERENCE' "$H" || fail "ALPHA_REFERENCE missing"
grep -q 'minWeight' "$H" || fail "minWeight missing"
grep -q 'usesPositiveViewDepth' "$H" || fail "usesPositiveViewDepth missing"
pass "oitWeightContract_t surface"

grep -q 'oit_weight_status' "$C" || fail "oit_weight_status missing"
grep -q 'oit_weight_validate' "$C" || fail "validate missing"
grep -q 'minWeight = 1e-2f' "$C" || fail "minWeight freeze missing"
grep -q 'maxWeight = 3e3f' "$C" || fail "maxWeight freeze missing"
grep -q 'OIT_WEIGHT_BOUNDED_PRODUCTION' "$C" || fail "production mode freeze missing"
pass "vk_oit_weight_contract implementation"

grep -q 'OitWeight_BoundedProduction' "$GLSL" || fail "GLSL helper missing"
grep -q 'oit_weight.glsl' "$ACCUM" || fail "oit_accum must include oit_weight.glsl"
grep -q 'OitWeight_BoundedProduction' "$ACCUM" || fail "oit_accum must call bounded weight"
pass "shader wiring"

grep -q 'OIT_CONTRACT_VERSION 2u\|OIT_CONTRACT_VERSION 2' "$OIT_H" || fail "oit contract must bump to v2 for weight mode"
grep -q 'vk_oit_weight_contract.h' "$OIT_H" || fail "oit contract must include weight contract"
grep -q 'OIT_WEIGHT_BOUNDED_PRODUCTION' "$H" || fail "bounded weight enum missing"
pass "oitContract_t links weight mode"

grep -q 'r_oitClassify.*, "1"' "$INIT" || fail "r_oitClassify production default must be 1"
grep -q 'VK_XPARENT_MODULATE' "$BACK" || fail "backend must exclude modulate from OIT"
grep -q 'colorWriteMask = 0' "$PIPE" || fail "additive pipeline must disable reveal write"
grep -q 'additive bucket\|Phase 2.5 routing' "$ROUTE" || fail "transparency_route_status must document 2.5 routing"
pass "additive separation + blend routing"

grep -q 'BOUNDED_PRODUCTION\|bounded_production' "$DOC" || fail "doc mode"
grep -q 'oit_weight_status' "$DOC" || fail "doc status"
grep -q 'usesPositiveViewDepth\|positive view-depth' "$DOC" || fail "doc view-depth"
pass "WBOIT_WEIGHT_CONTRACT.md"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All OIT weight / Phase 2.5 checks passed."
