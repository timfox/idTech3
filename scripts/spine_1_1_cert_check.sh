#!/usr/bin/env bash
# Static Spine 1.1 certification contracts (WBOIT × Temporal Reconstruction × weapon-after).
# Does not change boot defaults; GPU soak is optional via spine_1_1_lifecycle_stress.sh.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

echo "=== Spine 1.1 certification static check ==="

STABLE="$ROOT/config/modern_vulkan_stable.cfg"
CERT="$ROOT/config/vulkan_overlay_spine_1_1_cert.cfg"
TEMPORAL="$ROOT/config/vulkan_overlay_temporal_recon.cfg"
REG_C="$ROOT/renderers/vulkan/vk_pass_registry.c"
REG_H="$ROOT/renderers/vulkan/vk_pass_registry.h"
FRAME="$ROOT/renderers/vulkan/vk_frame_end.c"
DESC="$ROOT/renderers/vulkan/vk_descriptor_sets.c"
OIT="$ROOT/renderers/vulkan/vk_postfx_passes.c"
DIAG="$ROOT/renderers/vulkan/diagnostics/tr_init_diagnostics.inc"

[[ -f "$STABLE" ]] || fail "missing modern_vulkan_stable.cfg"
[[ -f "$CERT" ]] || fail "missing vulkan_overlay_spine_1_1_cert.cfg"
[[ -f "$TEMPORAL" ]] || fail "missing temporal recon overlay"

# Boot remains clustered raster shipping
grep -qE 'seta r_renderMode 3' "$STABLE" || fail "stable must remain mode 3"
grep -qE 'seta r_taa 0' "$STABLE" || fail "stable must keep TAA off"
grep -qE 'seta r_oit 1' "$STABLE" || fail "stable must keep production WBOIT on"
pass "boot stable remains mode3 + WBOIT, no TAA"

# Cert overlay pins
grep -q 'exec modern_clustered.cfg' "$CERT" || fail "cert must exec modern_clustered"
grep -q 'exec vulkan_overlay_temporal_recon.cfg' "$CERT" || fail "cert must exec temporal recon"
grep -qE 'seta r_oit 1' "$CERT" || fail "cert must pin WBOIT (r_oit 1)"
! grep -qE 'seta r_oit 2' "$CERT" || fail "cert must not pin MBOIT"
grep -qE 'seta r_spineCert 1' "$CERT" || fail "cert must enable r_spineCert 1"
grep -qE 'seta r_spineValidate 2' "$CERT" || fail "cert must enable r_spineValidate 2"
grep -qE 'seta r_bloom 1' "$CERT" || fail "cert must enable bloom"
grep -qE 'seta r_exposure_auto 1' "$CERT" || fail "cert must enable eye adapt"
grep -qE 'seta r_temporalWeaponAfterTaa 1' "$TEMPORAL" || fail "temporal overlay must pin weapon-after"
pass "cert overlay: mode3 stack + WBOIT + temporal + cert asserts"

# Combo promotion (no perpetual experimental violation on cert path)
grep -q 'spine_1_1_oit_taa_weapon' "$REG_C" || fail "missing Spine 1.1 certified combo id"
grep -q 'vk_spine_is_spine_1_1_combo' "$REG_H" "$REG_C" || fail "missing spine 1.1 combo API"
grep -q 'No violation — this is the certified ownership path' "$REG_C" || \
  fail "cert combo must not record perpetual OIT×TAA violation"
grep -q 'oit_x_taa_without_weapon_after' "$REG_C" || fail "soft-demote path must remain"
grep -q 'mboit_x_taa_experimental' "$REG_C" || fail "MBOIT×TAA must stay experimental"
pass "combo policy: cert promotes WBOIT path; soft-demote + MBOIT experimental remain"

# Resolved-OIT-only TAA invariant
grep -q 'vk_spine_cert_check_taa_input' "$REG_H" "$FRAME" || fail "TAA input cert check missing"
grep -q 'TAA current bound to raw OIT' "$REG_C" || fail "raw OIT TAA reject missing"
grep -q 'vk_spine_note_oit_skipped' "$OIT" || fail "skipped OIT fallback stamp missing"
grep -q 'vk_spine_cert_check_history_invalidated' "$REG_H" "$REG_C" || fail "history invalidate cert check missing"
grep -q 'vk_spine_cert_check_weapon_flush_order' "$REG_H" "$FRAME" || fail "weapon flush order cert check missing"
pass "TAA may consume only resolved world color (not raw OIT)"

# Descriptor rebound after attachment recreate
grep -q 'vk_spine_note_descriptors_rebound' "$REG_H" "$DESC" || fail "descriptor rebound API missing"
grep -q 'descriptorsPendingRebound' "$REG_C" || fail "pending rebound tracking missing"
grep -q 'attachments recreated without descriptor rebound' "$REG_C" || \
  fail "missing assert for unrebound descriptors"
pass "attachment recreate invalidates and rebuilds dependent descriptors"

# Weapon after temporal (ordering ownership)
grep -q 'RB_FlushDeferredWeaponAfterTaa' "$FRAME" || fail "weapon flush after TAA missing"
grep -q 'weapon pass wrote TAA history' "$REG_C" || fail "weapon→history ownership assert missing"
pass "weapon renders after world temporal reconstruction"

# Console entry points
grep -q 'renderer_spine_1_1_cert' "$DIAG" || fail "renderer_spine_1_1_cert command missing"
grep -q 'spine_1_1_stress' "$DIAG" || fail "spine_1_1_stress command missing"
grep -q 'spine_1_1_stress_report' "$DIAG" || fail "spine_1_1_stress_report missing"
pass "cert console commands present"

echo "=== Spine 1.1 certification static check PASSED ==="
echo "GPU: renderer_spine_1_1_cert; vid_restart; spine_1_1_stress 20 50 20"
echo "  or: bash scripts/spine_1_1_lifecycle_stress.sh (requires display + client)"
