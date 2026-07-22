#!/usr/bin/env bash
# Foundation Consolidation: Toksvig specular AA + debug cvar symbols.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/SPECULAR_AA.md"
CORE="$ROOT/renderers/vulkan/shaders/glsl/pbr_brdf_core.glsl"
INIT="$ROOT/renderers/vulkan/tr_init.c"

[[ -f "$DOC" ]] || fail "missing SPECULAR_AA.md"
grep -q 'Toksvig' "$DOC" || fail "doc must mention Toksvig"
grep -q 'r_specularAADebug' "$DOC" || fail "doc must mention r_specularAADebug"
grep -q 'r_pbr_specularAA' "$DOC" || fail "doc must mention r_pbr_specularAA"
pass "SPECULAR_AA.md symbols present"

grep -q 'PbrSpecularAARoughness' "$CORE" || fail "PbrSpecularAARoughness missing in pbr_brdf_core.glsl"
grep -q 'toksvig_roughness\|PbrSpecularAARoughness' "$CORE" || fail "toksvig roughness path missing in core"
pass "specular AA in shared BRDF core"

grep -q 'r_pbr_specularAA' "$INIT" || fail "r_pbr_specularAA cvar missing in tr_init.c"
grep -q 'r_pbr_specularAAStrength' "$INIT" || fail "r_pbr_specularAAStrength missing"
pass "specular AA cvars registered"

grep -q 'specularAA\|PbrSpecularAARoughness' "$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" || \
	fail "deferred_lighting_common must apply specular AA"
pass "deferred path applies specular AA"

if [[ -f "$ROOT/renderers/vulkan/vk_shading_compare.c" ]]; then
	grep -q 'r_specularAADebug' "$ROOT/renderers/vulkan/vk_shading_compare.c" || fail "r_specularAADebug missing"
	pass "r_specularAADebug cvar wired"
else
	pass "r_specularAADebug documented (vk_shading_compare optional)"
fi

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All specular AA checks passed."
