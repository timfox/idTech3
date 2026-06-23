#!/usr/bin/env bash
# Static wiring check: unified dynamic light color + Forward+ overflow shade.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

for sym in R_DynamicLightColor R_DynamicLightIntensityScale R_DynamicLightUsesLegacyScale; do
	grep -q "$sym" renderers/vulkan/tr_light.c || fail "missing $sym in tr_light.c"
	grep -q "$sym" renderers/vulkan/tr_local.h || fail "missing $sym in tr_local.h"
done

grep -q 'R_DynamicLightColor' renderers/vulkan/vk_forward_plus.c || fail 'Forward+ must use R_DynamicLightColor'
grep -q 'R_DynamicLightColor' renderers/vulkan/vk_volumetric_params.c || fail 'volumetric must use R_DynamicLightColor'
grep -q 'R_DynamicLightColor' renderers/vulkan/tr_shade.c || fail 'tr_shade must use R_DynamicLightColor'

grep -q 'r_dynamicLightScale' renderers/vulkan/tr_init.c || fail 'missing r_dynamicLightScale cvar'
grep -q 'r_lightGammaLink' renderers/vulkan/tr_init.c || fail 'missing r_lightGammaLink cvar'
grep -q 'r_forwardPlusOverflowShade' renderers/vulkan/tr_init.c || fail 'missing r_forwardPlusOverflowShade cvar'

grep -q 'pbrForwardPlus.x' renderers/vulkan/shaders/glsl/gen_frag.tmpl || fail 'gen_frag must use pbrForwardPlus.x overflow shade'
grep -q 'li >= 32u' renderers/vulkan/shaders/glsl/gen_frag.tmpl || fail 'gen_frag overflow light index guard'

grep -q 'R_WorldSHVertexColor' renderers/vulkan/tr_light.c || fail 'missing R_WorldSHVertexColor'
grep -q 'r_fogShadowSnap' renderers/vulkan/tr_init.c || fail 'missing r_fogShadowSnap cvar'
grep -q 'r_shWorldStrength' renderers/vulkan/tr_init.c || fail 'missing r_shWorldStrength cvar'
grep -q 'r_fogShadowSnap' renderers/vulkan/tr_backend.c || fail 'sun shadow texel snap missing in tr_backend.c'
grep -q 'R_StageUsesWorldSH' renderers/vulkan/tr_shade.c || fail 'world SH stage guard missing'

grep -q 'pbrSunShadowVisibility' renderers/vulkan/shaders/glsl/gen_frag.tmpl || fail 'gen_frag PBR sun shadow'
grep -q 'r_pbrSunShadow' renderers/vulkan/tr_init.c || fail 'missing r_pbrSunShadow cvar'
grep -q 'r_classicLighting' renderers/vulkan/tr_init.c || fail 'missing r_classicLighting cvar'
grep -q 'R_ClassicLightingActive()' renderers/vulkan/tr_shade.c || fail 'tr_shade must gate modern lighting on classic'
grep -q 'vk_forward_plus_update_sun_shadow_descriptor' renderers/vulkan/vk_forward_plus.c || fail 'sun shadow descriptor update missing'
grep -q 'VK_FillPbrSunShadowUniform' renderers/vulkan/tr_shade.c || fail 'PBR sun shadow uniform fill missing'

pass "dynamic light scale + Forward+ overflow wiring"
