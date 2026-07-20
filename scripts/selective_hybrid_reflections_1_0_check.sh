#!/usr/bin/env bash
# Selective Hybrid Reflections 1.0 static contracts.
# Does not change boot defaults; forbids diffuse RT GI / PT / FG in the SHR profile.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

echo "=== Selective Hybrid Reflections 1.0 check ==="

STABLE="$ROOT/config/modern_vulkan_stable.cfg"
MODERN="$ROOT/config/modern_vulkan.cfg"
PROFILE="$ROOT/config/vulkan_overlay_selective_hybrid_reflections.cfg"
DOC="$ROOT/docs/SELECTIVE_HYBRID_REFLECTIONS_1.0.md"
SHR_C="$ROOT/renderers/vulkan/vk_selective_reflection.c"
SHR_H="$ROOT/renderers/vulkan/vk_selective_reflection.h"
HYB="$ROOT/renderers/vulkan/extensions/rtx/vk_hybrid1.c"
SPEC_RGEN="$ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_spec.rgen"
SPEC_RCHIT="$ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_spec.rchit"
SSR="$ROOT/renderers/vulkan/shaders/glsl/ssr.frag"
COMP="$ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_composite.comp"
TEMP="$ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_temporal.comp"
POSTFX="$ROOT/renderers/vulkan/vk_postfx_passes.c"
DIAG="$ROOT/renderers/vulkan/tr_init_diagnostics.inc"
GEN_TMPL="$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl"

[[ -f "$STABLE" ]] || fail "missing modern_vulkan_stable.cfg"
grep -qE 'seta r_renderMode 2' "$STABLE" || fail "stable must remain mode 2"
grep -qE 'seta r_hybrid1 0' "$STABLE" || fail "stable must keep Hybrid1 off"
! grep -q 'r_selectiveHybridReflection 1' "$MODERN" "$STABLE" || fail "boot configs must not enable SHR"
pass "boot spine untouched"

[[ -f "$PROFILE" && -f "$DOC" && -f "$SHR_C" && -f "$SHR_H" ]] || fail "missing SHR sources/docs/profile"
grep -qE 'seta r_selectiveHybridReflection 1' "$PROFILE" || fail "profile must enable SHR"
grep -qE 'seta r_hybrid1_spec 1' "$PROFILE" || fail "profile must enable Hybrid1 specular"
grep -qE 'seta r_hybrid1_diffuse 0' "$PROFILE" || fail "profile must keep RT GI/diffuse off"
grep -qE 'seta r_pathtrace 0' "$PROFILE" || fail "profile must keep pathtrace off"
grep -qE 'seta r_aaMode 2' "$PROFILE" || fail "profile must keep SMAA"
! grep -qiE 'seta r_frameGen|seta r_dlssg' "$PROFILE" || fail "profile must not enable frame generation"
! grep -q 'r_selectiveHybridReflection 1' "$MODERN" || fail "modern_vulkan.cfg must not enable SHR"
pass "SHR profile: RT reflections, no GI/PT/FG; boot untouched"

grep -q 'vk_shr_rt_owns' "$HYB" || fail "Hybrid1 RT ownership gate missing"
grep -q 'vk_shr_ssr_allowed' "$POSTFX" || fail "SSR exclusive gate missing"
grep -q 'vk_shr_suppress_gen_frag_ibl_spec' "$ROOT/renderers/vulkan/tr_shade.c" || fail "gen_frag IBL suppress missing"
grep -q 'pbrDebugMode.w' "$GEN_TMPL" || grep -q 'pbrDebugMode\.w\|pbrDebugMode\[3\]' "$GEN_TMPL" || \
  grep -q 'DebugMode.w' "$GEN_TMPL" || fail "gen_frag must gate IBL specular via debug/suppress"
grep -q 'specRadiance.w = -clamp' "$SPEC_RCHIT" || fail "RT hit confidence packing missing"
grep -q 'hitConf' "$SPEC_RGEN" || fail "RT packed confidence in rgen missing"
grep -q 'hitConfidence' "$SSR" || fail "SSR confidence output missing"
grep -q 'shrMode' "$COMP" || fail "composite SHR mode missing"
grep -q 'probeSpecOcc\|probeSpecOcc\|probeSpec' "$COMP" || fail "probe specular occlusion path missing"
grep -q 'shsMode == 2u' "$TEMP" || fail "specular temporal SHR rejection missing"
grep -q 'r_shrFailInject' "$SHR_C" || fail "fail inject missing"
grep -q 'shr       :' "$DIAG" || fail "havenrp status SHR line missing"
grep -q 'hybrid1_spec_x_ssr_without_shr' "$ROOT/renderers/vulkan/vk_pass_registry.c" || fail "pass registry anti-stack missing"
grep -q 'ignoreIntersectionEXT' "$ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_spec.rahit" || fail "spec any-hit missing"
grep -q 'hybrid1.spec_rahit' "$HYB" || fail "spec any-hit pipeline wire missing"
pass "ownership router + confidence + composite + fail inject + status + alpha any-hit"

grep -q 'hist_spec\|temporal_spec' "$HYB" || fail "dedicated specular history missing"
grep -q 'taa_history' "$HYB" && fail "Hybrid1 must not reference taa_history" || true
pass "dedicated reflection history (not color TAA)"

echo "=== Selective Hybrid Reflections 1.0 check PASSED ==="
echo "GPU: exec vulkan_overlay_selective_hybrid_reflections.cfg; vid_restart"
echo "Recovery: exec modern_vulkan.cfg"
