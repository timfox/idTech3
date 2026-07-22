#!/usr/bin/env bash
# Ensure Forward+/Deferred/OIT/gen_frag share pbr_brdf_core.glsl.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "PASS: $*"; }

CORE="$ROOT/shaders/glsl/pbr_brdf_core.glsl"
VULKAN_CORE="$ROOT/renderers/vulkan/shaders/glsl/pbr_brdf_core.glsl"
[[ -f "$CORE" ]] || fail "missing canonical pbr_brdf_core.glsl"
[[ -e "$VULKAN_CORE" ]] || fail "missing Vulkan pbr_brdf_core.glsl link"
grep -q 'diffuse_burley' "$CORE" || fail "diffuse_burley missing"
grep -q 'ndf_ggx' "$CORE" || fail "ndf_ggx missing"
grep -q 'vis_smith_ggx' "$CORE" || fail "vis_smith_ggx missing"
grep -q 'fresnel_schlick' "$CORE" || fail "fresnel_schlick missing"
grep -q 'multiscatter_compensation' "$CORE" || fail "multiscatter_compensation missing"
grep -q 'clearcoat_lobe' "$CORE" || fail "clearcoat_lobe missing"
grep -q 'sheen_charlie' "$CORE" || fail "sheen_charlie missing"
grep -q 'toksvig_roughness' "$CORE" || fail "toksvig_roughness missing"
grep -q 'geometric_roughness' "$CORE" || fail "geometric_roughness missing"
grep -q 'PbrDiffuseBurley' "$CORE" || fail "PbrDiffuseBurley alias missing"
pass "pbr_brdf_core symbols present"

grep -q 'pbr_brdf_core' "$ROOT/renderers/vulkan/shaders/glsl/forward_plus_light_eval.glsl" || \
	fail "forward_plus_light_eval must include core"
grep -q 'pbr_brdf_core.glsl' "$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" || \
	fail "deferred_lighting_common must include core"
grep -q 'pbr_brdf_core' "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" || \
	fail "gen_frag.tmpl must reference pbr_brdf_core"
grep -q 'PbrDiffuseBurley\|PbrD_GGX\|PbrVisibilitySmithGGX\|Diffuse_Burley\|D_GGX\|CalcVisibility' \
	"$ROOT/renderers/vulkan/shaders/glsl/forward_plus_light_eval.glsl" || \
	fail "Forward+ eval must call shared BRDF wrappers"
pass "gen_frag + Forward+ + Deferred wired to pbr_brdf_core"

OCT="$ROOT/shaders/glsl/gbuffer_octahedral.glsl"
[[ -f "$OCT" ]] || fail "missing gbuffer_octahedral.glsl"
grep -q 'encode_octahedral' "$OCT" || fail "encode_octahedral missing"
grep -q 'decode_octahedral' "$OCT" || fail "decode_octahedral missing"
grep -q 'GbufEncodeOctahedral' "$OCT" || fail "GbufEncodeOctahedral alias missing"
grep -q 'gbuffer_octahedral.glsl' "$ROOT/renderers/vulkan/shaders/glsl/deferred_gbuffer_fill.comp" || \
	fail "deferred_gbuffer_fill must include octahedral helpers"
pass "gbuffer octahedral helpers present"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All pbr_brdf_core checks passed."
