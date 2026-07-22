#!/usr/bin/env bash
# Deferred Honesty Milestone 2 — MIXED_MATERIAL_DEFERRED naming + mixed export wiring.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/DEFERRED_HONESTY.md"
H="$ROOT/renderers/vulkan/vk_deferred_honesty.c"
HH="$ROOT/renderers/vulkan/vk_deferred_honesty.h"
GF="$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl"
LC="$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl"
CF="$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_composite.frag"

grep -q 'MIXED_MATERIAL_DEFERRED' "$DOC" || fail "doc must name MIXED_MATERIAL_DEFERRED"
grep -q 'HYBRID_ADDITIVE_DEFERRED' "$DOC" || fail "doc must keep HYBRID_ADDITIVE_DEFERRED"
grep -q 'lightmap' "$DOC" || fail "doc must mention lightmap ownership"
pass "DEFERRED_HONESTY.md M2 vocabulary"

grep -q 'MIXED_MATERIAL_DEFERRED' "$H" || fail "honesty.c must emit MIXED_MATERIAL_DEFERRED"
grep -q 'R_DeferredMixedMaterialWanted' "$HH" || fail "R_DeferredMixedMaterialWanted missing"
grep -q 'DEFERRED_ARCH_MIXED_MATERIAL' "$HH" || fail "DEFERRED_ARCH_MIXED_MATERIAL missing"
grep -q 'DEFERRED_OWNER_BIAS\|1024' "$HH" || fail "owner bias constant missing"
pass "honesty arch helpers"

grep -q 'deferredMixedHandoff' "$GF" || fail "gen_frag must gate mixed handoff"
grep -q 'staticLightExport' "$GF" || fail "gen_frag must export static light"
grep -q 'gbufferBaseColor' "$GF" || fail "gen_frag must export unlit base"
pass "gen_frag mixed export"

grep -q 'mixedMaterial' "$LC" || fail "lighting common must handle mixedMaterial"
grep -q 'staticTerm\|lightmapIrr' "$LC" || fail "lighting must apply lightmap static term"
grep -q 'mixedMaterial' "$CF" || fail "composite must handle mixed ownership"
pass "deferred lighting + composite mixed path"

grep -q 'pbrDebugMode\[1\] = 3' "$ROOT/renderers/vulkan/tr_shade.c" || \
	grep -q '3.0f' "$ROOT/renderers/vulkan/tr_shade.c" || fail "tr_shade must signal mixed handoff (y=3)"
pass "CPU handoff signal"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All deferred mixed-material checks passed."
