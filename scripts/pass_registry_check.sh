#!/usr/bin/env bash
# Static contract: lightweight Spine pass/resource registry (not a frame-graph).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

echo "=== Pass/resource registry check ==="

REG_H="$ROOT/renderers/vulkan/vk_pass_registry.h"
REG_C="$ROOT/renderers/vulkan/vk_pass_registry.c"
SCENE="$ROOT/renderers/vulkan/vk_scene_pass.c"
FRAME="$ROOT/renderers/vulkan/vk_frame_submit.c"
ATT="$ROOT/renderers/vulkan/vk_attachments.c"
INIT="$ROOT/renderers/vulkan/vk_init_device.c"
SHUT="$ROOT/renderers/vulkan/vk_shutdown.c"

[[ -f "$REG_H" && -f "$REG_C" ]] || fail "vk_pass_registry.{h,c} missing"

# Core enums / APIs
grep -q 'VK_SPINE_PHASE_FRAME_BEGIN' "$REG_H" || fail "phase model missing"
grep -q 'VK_SPINE_PASS_WORLD_OPAQUE' "$REG_H" || fail "world_opaque pass id missing"
grep -q 'VK_SPINE_PASS_TEMPORAL_RECON' "$REG_H" || fail "temporal_recon pass id missing"
grep -q 'VK_SPINE_PASS_WEAPON' "$REG_H" || fail "weapon pass id missing"
grep -q 'VK_SPINE_RES_HDR_COLOR' "$REG_H" || fail "hdr_color resource id missing"
grep -q 'VK_SPINE_ACCESS_HISTORY_READ' "$REG_H" || fail "history access kinds missing"
grep -q 'vk_spine_status_f' "$REG_H" || fail "spine_status API missing"
grep -q 'r_spineValidate' "$REG_C" || fail "r_spineValidate cvar missing"
pass "phase/pass/resource enums + status API"

# Minimum production spine passes registered
for p in FRAME_PREP LIGHT_PACK TILE_CONSTRUCT SUN_SHADOW WORLD_OPAQUE GBUFFER_FILL \
  DEFERRED_LIGHTING FORWARD_PLUS_OPAQUE SSR AMBIENT_VISIBILITY FROXEL_VOLUME \
  TRANSPARENT_FORWARD_PLUS WBOIT_ACCUM MBOIT_MOMENTS MBOIT_ACCUM OIT_RESOLVE \
  REACTIVE_MASK TEMPORAL_RECON SMAA BLOOM EYE_ADAPTATION WEAPON HUD_2D \
  PRESENTATION HISTORY_MAINT; do
  grep -q "VK_SPINE_PASS_${p}" "$REG_H" || fail "missing pass id VK_SPINE_PASS_${p}"
done
pass "minimum production spine passes declared"

# Must validate, not only name-log
grep -q 'vk_spine_record_violation' "$REG_C" || fail "registry must record violations"
grep -q 'stale generation' "$REG_C" || fail "stale generation validation missing"
grep -q 'phase regression' "$REG_C" || fail "phase-order validation missing"
grep -q 'dual_ao_ssao_and_av' "$REG_C" || fail "dual AO ownership check missing"
grep -q 'oit_x_taa' "$REG_C" || fail "OIT×TAA combo check missing"
grep -q 'oit_x_taa_without_weapon_after' "$REG_C" || fail "OIT×TAA must require weapon-after-TAA"
grep -q 'vk_spine_note_clear' "$REG_H" "$REG_C" || fail "clear contract API missing"
grep -q 'vk_spine_note_barrier' "$REG_H" "$REG_C" || fail "barrier contract API missing"
grep -q 'vk_spine_note_layout' "$REG_H" "$REG_C" || fail "layout stamp API missing"
grep -q 'vk_spine_expect_layout' "$REG_H" "$REG_C" || fail "layout expectation API missing"
grep -q 'layoutKnown' "$REG_C" || fail "layoutKnown tracking missing"
grep -q 'OIT resolve read' "$REG_C" || fail "OIT resolve-without-clear validation missing"
pass "validation (generation / phase / feature combos / clear-barrier / layout)"

# Wired into real renderer paths
grep -q 'vk_spine_pass_begin_named' "$SCENE" || fail "pass_diag must feed registry"
grep -q 'vk_spine_dump_device_lost' "$SCENE" || fail "DEVICE_LOST must dump spine"
grep -q 'vk_spine_frame_begin' "$FRAME" || fail "frame_begin must start spine frame"
grep -q 'vk_spine_frame_end' "$FRAME" || fail "frame_end must close spine frame"
grep -q 'vk_spine_attachments_created' "$ATT" || fail "create_attachments must stamp resources"
grep -q 'vk_spine_attachments_destroyed' "$ATT" || fail "destroy_attachments must clear resources"
grep -q 'vk_spine_registry_init' "$INIT" || fail "vk_initialize must init registry"
grep -q 'vk_spine_registry_shutdown' "$SHUT" || fail "vk_shutdown must shutdown registry"
pass "hooks: pass_diag / frame / attachments / init / shutdown"

# Instrumented production passes
grep -q 'VK_SPINE_PASS_BLOOM' "$FRAME" || fail "bloom must be observed"
grep -q 'VK_SPINE_PASS_TEMPORAL_RECON' "$FRAME" || fail "temporal recon must be observed"
grep -q 'VK_SPINE_PASS_WEAPON' "$ROOT/renderers/vulkan/tr_backend.c" || fail "weapon flush must be observed"
grep -q 'VK_SPINE_PASS_AMBIENT_VISIBILITY' "$ROOT/renderers/vulkan/vk_ambient_visibility.c" || \
  fail "AV apply must be observed"
grep -q 'VK_SPINE_PASS_OIT_RESOLVE' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || \
  fail "OIT must be observed"
grep -q 'VK_SPINE_PASS_GBUFFER_FILL' "$ROOT/renderers/vulkan/vk_deferred_gbuffer.c" || \
  fail "G-buffer fill must be observed"
grep -q 'VK_SPINE_PASS_LIGHT_PACK' "$ROOT/renderers/vulkan/vk_forward_plus.c" || \
  fail "Forward+ light pack must be observed"
grep -q 'VK_SPINE_PASS_SMAA' "$ROOT/renderers/vulkan/vk_post_aa.c" || \
  fail "SMAA must be observed"
grep -q 'VK_SPINE_PASS_SUN_SHADOW' "$ROOT/renderers/vulkan/vk_sun_shadow_pass.c" || \
  fail "sun shadow must be observed"
grep -q 'VK_SPINE_PASS_FROXEL_VOLUME' "$ROOT/renderers/vulkan/vk_volumetric_pass_compute.c" || \
  fail "froxel volume must be observed"
grep -q 'vk_spine_note_clear' "$ROOT/renderers/vulkan/vk_render_pass.c" || \
  fail "OIT RP clears must stamp spine clear"
grep -q 'vk_spine_note_clear' "$ROOT/renderers/vulkan/vk_reactive_mask.c" || \
  fail "reactive mask clear must stamp spine clear"
grep -q 'vk_spine_note_barrier' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || \
  fail "OIT sample barrier must stamp spine barrier"
grep -q 'vk_spine_note_layout' "$ROOT/renderers/vulkan/vk_image_layout.c" || \
  fail "depth layout transitions must stamp spine layout"
grep -q 'vk_spine_expect_layout' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || \
  fail "OIT resolve must expect sample layouts"
grep -q 'vk_spine_expect_layout' "$ROOT/renderers/vulkan/vk_ambient_visibility.c" || \
  fail "AV must expect depth read-only layout after transition"
grep -q 'vk_spine_expect_layout' "$ROOT/renderers/vulkan/vk_frame_end.c" || \
  fail "TAA must expect HDR/history/reactive layouts"
grep -q 'gamma_src' "$ROOT/renderers/vulkan/vk_frame_end.c" || \
  fail "gamma must expect HDR sample layout"
grep -q 'vk_spine_expect_layout' "$ROOT/renderers/vulkan/vk_post_aa.c" || \
  fail "SMAA must expect HDR sample layout"
grep -q 'bloom_extract' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || \
  fail "bloom must expect HDR sample layout"
grep -q 'VK_SPINE_RES_SSR' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || \
  fail "SSR must stamp SSR resource layout"
grep -q 'vk_spine_expect_layout' "$ROOT/renderers/vulkan/vk_volumetric_pass_compute.c" || \
  fail "froxel must expect sun-shadow sample layout"
grep -q 'HISTORY_READ' "$ROOT/renderers/vulkan/vk_frame_end.c" || \
  fail "TAA must stamp HISTORY_READ only when history valid"
grep -q 'vk_spine_note_temporal_history' "$REG_C" || fail "frame_begin must sync TAA history validity"
grep -q 'vk_spine_combo_suppress_taa' "$REG_H" "$REG_C" || fail "OIT×TAA soft-demote API missing"
grep -q 'vk_spine_is_spine_1_1_combo' "$REG_H" "$REG_C" || fail "Spine 1.1 combo API missing"
grep -q 'vk_spine_note_descriptors_rebound' "$REG_H" "$REG_C" || fail "descriptor rebound API missing"
grep -q 'vk_spine_cert_check_taa_input' "$REG_H" "$REG_C" || fail "TAA input cert check missing"
grep -q 'suppressTaaThisFrame' "$REG_C" || fail "combo soft-demote state missing"
grep -q 'taa_suppress_illegal_combo' "$ROOT/renderers/vulkan/vk_frame_end.c" || \
  fail "TAA must observe spine combo soft-demote"
grep -q 'taa_enter' "$ROOT/renderers/vulkan/vk_frame_end.c" || fail "TAA pass_diag stage missing"
grep -q 'gamma_enter' "$ROOT/renderers/vulkan/vk_frame_end.c" || fail "gamma pass_diag stage missing"
grep -q 'luminance_enter' "$ROOT/renderers/vulkan/vk_frame_end.c" || fail "luminance pass_diag stage missing"
grep -q 'latePost oit=' "$ROOT/renderers/vulkan/vk_scene_pass.c" || \
  fail "DEVICE_LOST dump must include late-post feature context"
grep -q 'postChainWriter' "$ROOT/renderers/vulkan/vk_scene_pass.c" || \
  fail "DEVICE_LOST dump must include post-chain writer"
pass "instrumented spine passes (bloom/TAA/weapon/AV/OIT/G-buffer/Forward+/SMAA/shadow/froxel/layout)"

# OIT × TAA × weapon matrix ownership (profiles)
QUALITY="$ROOT/config/modern_vulkan_quality.cfg"
STABLE="$ROOT/config/modern_vulkan_stable.cfg"
grep -qE 'seta r_oit 1' "$QUALITY" || fail "quality expects WBOIT (r_oit 1)"
grep -qE 'seta r_taa 0' "$QUALITY" || fail "quality must keep TAA off while OIT is on (Spine matrix)"
grep -q 'r_temporalWeaponAfterTaa' "$ROOT/renderers/vulkan/vk_aa_policy.c" || \
  fail "weapon-after-TAA cvar missing for OIT×TAA matrix"
# Temporal overlay must not silently turn on OIT with TAA without weapon deferral default
OVERLAY="$ROOT/config/vulkan_overlay_temporal_recon.cfg"
if grep -qE 'seta r_oit [12]' "$OVERLAY" 2>/dev/null; then
  grep -q 'r_temporalWeaponAfterTaa 1' "$OVERLAY" || \
    fail "temporal overlay enabling OIT must force r_temporalWeaponAfterTaa 1"
fi
pass "OIT×TAA×weapon profile matrix (quality keeps OIT without TAA)"

# Stable profile untouched by this change
grep -q 'seta r_taa 0' "$STABLE" || fail "stable must keep r_taa 0"
pass "stable profile ownership unchanged"

echo "=== Pass/resource registry check PASSED ==="
echo "Runtime: pass_registry_status / spine_status ; set r_spineValidate 1|2"
echo "DEVICE_LOST dump includes [VK][device_lost][spine] lines."
