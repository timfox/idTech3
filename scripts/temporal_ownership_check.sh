#!/usr/bin/env bash
# Static contract: shared temporal history ownership (Spine 1.0).
# Does not enable TAA on stable; does not change profile ownership.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

echo "=== Temporal ownership check ==="

TEMP_C="$ROOT/renderers/vulkan/vk_temporal.c"
TEMP_H="$ROOT/renderers/vulkan/vk_temporal.h"
BE="$ROOT/renderers/vulkan/tr_backend.c"
PRES="$ROOT/renderers/vulkan/vk_presentation.c"
BSP="$ROOT/renderers/vulkan/tr_bsp.c"
INIT="$ROOT/renderers/vulkan/tr_init.c"
DIAG="$ROOT/renderers/vulkan/tr_init_diagnostics.inc"
FRAME="$ROOT/renderers/vulkan/vk_frame_end.c"

[[ -f "$TEMP_C" && -f "$TEMP_H" ]] || fail "missing vk_temporal sources"

# Shared reset API + consumers
grep -q 'vk_temporal_request_sticky_reset' "$TEMP_H" || fail "sticky reset API missing"
grep -q 'vk_temporal_status_f' "$TEMP_H" || fail "temporal_status API missing"
grep -q 'vk_reset_taa_history' "$TEMP_C" || fail "TAA history reset missing"
grep -q 'vk_ambient_visibility_reset_history' "$TEMP_C" || fail "apply_resets must reset AV history"
grep -q 'vk_reset_volumetric_history' "$TEMP_C" || fail "apply_resets must reset volumetric history"
grep -q 'vk_reset_motion_history' "$TEMP_C" || fail "apply_resets must reset motion history"
grep -q 'vk_reset_occlusion_visibility' "$TEMP_C" || fail "apply_resets must reset occlusion visibility"
pass "shared apply_resets invalidates TAA/AV/volumetric/motion/occlusion"

# Weapon / RDF_NOWORLDMODEL thrash guard
grep -q 'RDF_NOWORLDMODEL flips every frame' "$TEMP_C" || \
  fail "camera-cut path must document NOWORLDMODEL thrash guard"
python3 - <<'PY' || exit 1
from pathlib import Path
text = Path("renderers/vulkan/vk_temporal.c").read_text()
fn = text.split("vk_temporal_compute_shared_camera_cut", 1)[1].split("void vk_temporal_begin_frame", 1)[0]
if "noWorldTransition" not in fn:
    raise SystemExit("FAIL: noWorldTransition ownership missing")
if "doneWorldScene" not in fn:
    raise SystemExit("FAIL: weapon flicker must gate on doneWorldScene")
# After world is done, NOWORLDMODEL flip must be suppressed
if "noWorldTransition = qfalse" not in fn:
    raise SystemExit("FAIL: NOWORLDMODEL transition must be suppressed after world scene")
print("PASS: NOWORLDMODEL after world does not thrash temporal history")
PY

# World matrix capture / commit ownership
grep -q 'vk_temporal_capture_world_viewparms' "$TEMP_C" "$BE" || \
  fail "world viewparms must be captured before weapon overwrite"
grep -q 'worldMatricesCaptured' "$TEMP_C" || fail "commit must prefer worldMatricesCaptured"
grep -q 'portalView != PV_NONE' "$TEMP_C" || fail "portal views must not own world matrix history"
pass "world matrix capture + portal isolation"

# Weapon after TAA
grep -q 'RB_TryDeferWeaponDrawSurfs' "$BE" || fail "weapon defer entry missing"
grep -q 'RB_FlushDeferredWeaponAfterTaa' "$BE" "$FRAME" || fail "weapon flush after TAA missing"
grep -q 'vk_temporal_want_weapon_after_taa' "$TEMP_C" "$BE" || fail "weapon-after-TAA gate missing"
grep -q 'r_temporalWeaponAfterTaa' "$ROOT/renderers/vulkan/vk_aa_policy.c" || \
  fail "r_temporalWeaponAfterTaa cvar missing"
pass "weapon deferred until after world TAA"

# Sticky ownership sites
grep -q 'VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE' "$PRES" || fail "presentation must sticky-reset on swapchain"
grep -q 'VK_TEMPORAL_RESET_WORLD_CHANGE' "$BSP" || fail "map load must sticky-reset world change"
pass "sticky reset sites: presentation + world load"

# Console / diagnostics
grep -q 'temporal_status' "$INIT" || fail "temporal_status command must be registered"
grep -q 'vk_temporal_status_f' "$TEMP_C" || fail "temporal_status implementation missing"
grep -q 'weaponAfter' "$DIAG" || fail "havenrp_renderer_status must report weaponAfter"
grep -q 'vk_temporal_reconstruction_wanted' "$DIAG" || fail "havenrp temporal line must report recon"
pass "temporal_status + havenrp temporal ownership dump"

# Stable profile must not require TAA (ownership stays SMAA on stable)
STABLE="$ROOT/config/modern_vulkan_stable.cfg"
if grep -qE 'seta r_taa 1' "$STABLE"; then
  fail "stable must not enable r_taa 1 (SMAA is Spine presentation AA)"
fi
pass "stable temporal owner remains non-TAA"

echo "=== Temporal ownership check PASSED ==="
echo "Manual GPU: temporal_status during look/weapon/vid_restart/map change;"
echo "  r_temporalDebug 2; r_taa 1 + r_temporalWeaponAfterTaa 1 weapon silhouette check."
