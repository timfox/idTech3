#!/usr/bin/env bash
# Static Spine combination matrix: stable / quality / temporal / safe ownership.
# Does not enable experimental RT/Hybrid1 as shipping defaults.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }
cvar_is() { # file var expected
  grep -qE "seta $2 $3(\\s|$)" "$1" || return 1
}

echo "=== Spine combination matrix check ==="

STABLE="$ROOT/config/modern_vulkan_stable.cfg"
QUALITY="$ROOT/config/modern_vulkan_quality.cfg"
TEMPORAL="$ROOT/config/vulkan_overlay_temporal_recon.cfg"
SAFE="$ROOT/config/gfx_safe.cfg"
REG_C="$ROOT/renderers/vulkan/vk_pass_registry.c"
AA="$ROOT/renderers/vulkan/vk_aa_policy.c"

[[ -f "$STABLE" && -f "$QUALITY" && -f "$TEMPORAL" && -f "$SAFE" ]] || fail "missing profile cfgs"

# --- Stable shipping spine ---
cvar_is "$STABLE" r_renderMode 2 || fail "stable expects Forward+ renderMode 2"
cvar_is "$STABLE" r_taa 0 || fail "stable must keep TAA off"
cvar_is "$STABLE" r_oit 0 || fail "stable must keep OIT off"
cvar_is "$STABLE" r_aaMode 2 || fail "stable expects SMAA (r_aaMode 2)"
cvar_is "$STABLE" r_ssao 0 || fail "stable must keep legacy SSAO off when AV owns AO"
# One AO owner: GTAO mode 2 (documented Spine table); do not require mode 4
cvar_is "$STABLE" r_ambientVisibilityMode 2 || fail "stable AO owner expected mode 2 (GTAO)"
grep -qE 'seta r_hybrid1 0|seta r_rtx 0' "$STABLE" || true
# RT off if present
if grep -q 'seta r_rtx ' "$STABLE"; then
  cvar_is "$STABLE" r_rtx 0 || fail "stable must not enable r_rtx"
fi
if grep -q 'seta r_hybrid1 ' "$STABLE"; then
  cvar_is "$STABLE" r_hybrid1 0 || fail "stable must not enable Hybrid1"
fi
pass "stable matrix: Forward+ + SMAA + GTAO, no TAA/OIT/RT"

# --- Quality overlay ---
grep -q 'exec modern_vulkan_stable.cfg' "$QUALITY" || fail "quality must exec stable base"
cvar_is "$QUALITY" r_oit 1 || fail "quality expects WBOIT"
cvar_is "$QUALITY" r_taa 0 || fail "quality must keep TAA off while OIT on"
cvar_is "$QUALITY" r_ssr 1 || fail "quality expects SSR"
cvar_is "$QUALITY" r_volumetricFog 1 || fail "quality expects froxel fog"
pass "quality matrix: stable + WBOIT + SSR + froxel, TAA off"

# --- Temporal reconstruction overlay ---
cvar_is "$TEMPORAL" r_taa 1 || fail "temporal overlay expects r_taa 1"
cvar_is "$TEMPORAL" r_taaMotionVectors 1 || fail "temporal overlay expects motion vectors"
cvar_is "$TEMPORAL" r_aaMode 4 || fail "temporal overlay expects r_aaMode 4 (no post-TAA SMAA cleanup halo)"
cvar_is "$TEMPORAL" r_temporalSmaaCleanup 0 || fail "temporal overlay must keep r_temporalSmaaCleanup 0"
cvar_is "$TEMPORAL" r_temporalHistoryWeight 0.68 || fail "temporal overlay expects conservative historyWeight 0.68"
# Must not enable OIT in the temporal overlay (stacking is experimental)
if grep -qE 'seta r_oit [12]' "$TEMPORAL"; then
  fail "temporal overlay must not enable OIT (use quality or explicit experimental stack)"
fi
# Weapon-after-TAA default is 1 in code; overlay should pin it for matrix clarity
if ! grep -q 'r_temporalWeaponAfterTaa' "$TEMPORAL"; then
  fail "temporal overlay must seta r_temporalWeaponAfterTaa 1 (weapon history ownership)"
fi
cvar_is "$TEMPORAL" r_temporalWeaponAfterTaa 1 || fail "temporal overlay weapon-after must be 1"
# Heuristic reactive (history bleed / near-weapon) must not be gated only on stamp mask
TAA_FRAG="$ROOT/renderers/vulkan/shaders/glsl/taa.frag"
grep -q 'historyBleed' "$TAA_FRAG" || fail "taa.frag missing historyBleed anti-echo reject"
grep -q 'Heuristic reactive always runs' "$TAA_FRAG" || fail "taa.frag must keep heuristic reactive independent of stamp mask"
pass "temporal overlay: TAA+MVs, aaMode 4, no cleanup halo, weapon-after pinned"

# --- Unified Clustered (opt-in mode 3; must not displace Spine stable mode 2) ---
CLUSTERED="$ROOT/config/modern_clustered.cfg"
UC_OVERLAY="$ROOT/config/vulkan_overlay_unified_clustered.cfg"
GEN_FRAG="$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl"
OIT_ACCUM="$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag"
OIT_MBOIT="$ROOT/renderers/vulkan/shaders/glsl/oit_accum_mboit.frag"
[[ -f "$CLUSTERED" && -f "$UC_OVERLAY" ]] || fail "missing modern_clustered / unified clustered overlay"
cvar_is "$STABLE" r_renderMode 2 || fail "stable must remain renderMode 2 (clustered is opt-in)"
cvar_is "$CLUSTERED" r_renderMode 3 || fail "modern_clustered expects r_renderMode 3"
cvar_is "$CLUSTERED" r_forwardPlusZSlices 8 || fail "modern_clustered expects ZSlices 8"
cvar_is "$CLUSTERED" r_taa 0 || fail "modern_clustered must keep TAA off (SMAA baseline)"
cvar_is "$UC_OVERLAY" r_renderMode 3 || fail "unified clustered overlay expects r_renderMode 3"
cvar_is "$UC_OVERLAY" r_forwardPlusZSlices 8 || fail "unified clustered overlay expects ZSlices 8"
grep -q 'forward_plus_cluster.glsl' "$GEN_FRAG" || fail "gen_frag.tmpl must include forward_plus_cluster.glsl"
grep -q 'fp_cluster_index' "$GEN_FRAG" || fail "gen_frag.tmpl must use fp_cluster_index"
grep -q 'forward_plus_cluster.glsl' "$OIT_ACCUM" || fail "oit_accum.frag must include forward_plus_cluster.glsl"
grep -q 'fp_cluster_index' "$OIT_ACCUM" || fail "oit_accum.frag must use fp_cluster_index"
grep -q 'forward_plus_cluster.glsl' "$OIT_MBOIT" || fail "oit_accum_mboit.frag must include forward_plus_cluster.glsl"
grep -q 'fp_cluster_index' "$OIT_MBOIT" || fail "oit_accum_mboit.frag must use fp_cluster_index"
grep -q 'Soft-cap Forward+ specular' "$GEN_FRAG" || fail "gen_frag.tmpl missing Forward+ specular soft-cap"
pass "clustered opt-in: mode 3 + ZSlices 8, shared cluster GLSL, soft-cap; stable stays mode 2"

# --- Safe recovery ---
cvar_is "$SAFE" r_taa 0 || fail "gfx_safe must disable TAA"
cvar_is "$SAFE" r_oit 0 || fail "gfx_safe must disable OIT"
cvar_is "$SAFE" r_ambientVisibilityMode 0 || fail "gfx_safe must disable AV"
cvar_is "$SAFE" r_ssao 0 || fail "gfx_safe must disable SSAO"
cvar_is "$SAFE" r_aaMode 0 || fail "gfx_safe must disable post AA"
pass "gfx_safe recovery: no TAA/OIT/AO/post-AA"

# --- Runtime combo validators exist ---
grep -q 'dual_ao_ssao_and_av' "$REG_C" || fail "runtime dual-AO validator missing"
grep -q 'oit_x_taa_without_weapon_after' "$REG_C" || fail "runtime OIT×TAA×weapon validator missing"
grep -q 'suppressing world TAA this frame' "$REG_C" || fail "OIT×TAA without weapon-after must soft-demote TAA"
grep -q 'taa_without_weapon_after' "$REG_C" || fail "runtime TAA×weapon validator missing"
grep -q 'r_temporalWeaponAfterTaa' "$AA" || fail "weapon-after-TAA cvar registration missing"
grep -q 'vk_spine_expect_layout' "$REG_C" || fail "layout expectation API missing from registry"
pass "runtime combo + layout expectation validators present"

# --- Spine 1.1 opt-in cert (must not displace shipping matrix) ---
CERT="$ROOT/config/vulkan_overlay_spine_1_1_cert.cfg"
[[ -f "$CERT" ]] || fail "missing vulkan_overlay_spine_1_1_cert.cfg"
cvar_is "$CERT" r_oit 1 || fail "Spine 1.1 cert expects WBOIT"
cvar_is "$CERT" r_spineCert 1 || fail "Spine 1.1 cert expects r_spineCert 1"
grep -q 'spine_1_1_oit_taa_weapon' "$REG_C" || fail "Spine 1.1 certified combo id missing"
grep -q 'vk_spine_is_spine_1_1_combo' "$REG_C" || fail "Spine 1.1 combo detector missing"
# Shipping defaults still must not enable OIT+TAA together
cvar_is "$STABLE" r_renderMode 2 || fail "stable must remain mode 2 after Spine 1.1"
pass "Spine 1.1 cert overlay present; shipping matrix unchanged"

# --- Spine 1.2 mode model (opt-in; must not displace Tier A) ---
bash "$ROOT/scripts/spine_1_2_mode_check.sh"


# --- Illegal stacks must not be defaults ---
# quality∩temporal must not be a single default cfg that enables both OIT and TAA
if grep -qE 'seta r_oit [12]' "$QUALITY" && grep -qE 'seta r_taa 1' "$QUALITY"; then
  fail "quality must not enable OIT and TAA together"
fi
pass "no shipping cfg enables OIT+TAA together"

echo "=== Spine combination matrix check PASSED ==="
echo "Manual GPU: exec modern_vulkan_stable.cfg | quality | temporal_recon | modern_clustered | gfx_safe;"
echo "  Spine 1.1 cert: exec vulkan_overlay_spine_1_1_cert.cfg; vid_restart; spine_1_1_stress"
echo "  illegal OIT+TAA without weapon-after still soft-demotes TAA."
