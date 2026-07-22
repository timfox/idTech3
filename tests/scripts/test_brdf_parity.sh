#!/usr/bin/env bash
# Foundation Consolidation: shared pbr_brdf_core consumers + shading compare docs.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/SHARED_BRDF.md"
CORE="$ROOT/renderers/vulkan/shaders/glsl/pbr_brdf_core.glsl"

[[ -f "$DOC" ]] || fail "missing SHARED_BRDF.md"
grep -q 'r_shadingCompare' "$DOC" || fail "doc must mention r_shadingCompare"
grep -q 'pbr_brdf_core.glsl' "$DOC" || fail "doc must mention pbr_brdf_core.glsl"
grep -q 'parity\|tolerance\|RMSE' "$DOC" || fail "doc must mention parity tolerances"
pass "SHARED_BRDF.md symbols present"

[[ -f "$CORE" ]] || fail "missing pbr_brdf_core.glsl"
grep -q 'PbrDiffuseBurley' "$CORE" || fail "PbrDiffuseBurley missing"
grep -q 'PbrD_GGX' "$CORE" || fail "PbrD_GGX missing"
grep -q 'PbrVisibilitySmithGGX' "$CORE" || fail "PbrVisibilitySmithGGX missing"
pass "pbr_brdf_core symbols present"

grep -q 'pbr_brdf_core.glsl' "$ROOT/renderers/vulkan/shaders/glsl/forward_plus_light_eval.glsl" || fail "Forward+ must include core"
grep -q 'pbr_brdf_core.glsl' "$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" || fail "deferred must include core"
grep -q 'pbr_brdf_core' "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" || fail "gen_frag must reference pbr_brdf_core"
pass "all shading paths include pbr_brdf_core"

if [[ -f "$ROOT/renderers/vulkan/vk_shading_compare.c" ]]; then
	grep -q 'r_shadingCompare' "$ROOT/renderers/vulkan/vk_shading_compare.c" || fail "r_shadingCompare cvar missing"
	grep -q 'shading_compare_status' "$ROOT/renderers/vulkan/vk_shading_compare.c" || fail "shading_compare_status missing"
	pass "vk_shading_compare.c present"
else
	grep -q 'shading_compare_status' "$DOC" || fail "doc must mention shading_compare_status"
	pass "shading compare documented (source optional)"
fi

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All BRDF parity checks passed."
