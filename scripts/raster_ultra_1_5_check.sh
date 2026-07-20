#!/usr/bin/env bash
# Static gate: Raster Ultra 1.5 Present-Time Adaptive Reconstruction.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
fail=0

need() {
  local f="$1"
  if [[ ! -f "$ROOT/$f" ]]; then
    echo "FAIL missing: $f"
    fail=1
  else
    echo "OK  $f"
  fi
}

need "renderers/vulkan/vk_present_recon.c"
need "renderers/vulkan/vk_present_recon.h"
need "renderers/vulkan/shaders/glsl/taa.frag"
need "docs/RASTER_ULTRA_1.5.md"
need "config/vulkan_overlay_present_adaptive_recon.cfg"
need "config/modern_raster_ultra.cfg"

# Mode 3 must be Present-Time Adaptive (not legacy SMAA T2x scaffold)
grep -q 'Present-Time Adaptive Reconstruction' "$ROOT/renderers/vulkan/vk_aa_policy.c" || {
  echo "FAIL aa_policy mode 3 not remapped to Present-Time Adaptive"
  fail=1
}
grep -q 'migrated from SMAA T2x' "$ROOT/renderers/vulkan/vk_aa_policy.c" || {
  echo "FAIL missing mode 3 migration note"
  fail=1
}
grep -q 'spatialCurrentFallback' "$ROOT/renderers/vulkan/shaders/glsl/taa.frag" || {
  echo "FAIL taa.frag missing adaptive spatial fallback"
  fail=1
}
grep -q 'vk_present_recon_wants_adaptive' "$ROOT/renderers/vulkan/vk_postfx_params.c" || {
  echo "FAIL postfx missing adaptive history caps"
  fail=1
}
grep -q 'vk_present_recon_begin_frame' "$ROOT/renderers/vulkan/vk_frame_submit.c" || {
  echo "FAIL latency begin_frame hook missing"
  fail=1
}
grep -q 'present_recon_status' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL present_recon_status command not registered"
  fail=1
}
grep -q 'motion_vector_cert' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL motion_vector_cert command not registered"
  fail=1
}

# Ultra default must remain SMAA (certified zero-history)
grep -q 'seta r_aaMode 2' "$ROOT/config/modern_raster_ultra.cfg" || {
  echo "FAIL ultra must keep r_aaMode 2 (SMAA)"
  fail=1
}
grep -q 'seta r_taa 0' "$ROOT/config/modern_raster_ultra.cfg" || {
  echo "FAIL ultra must keep r_taa 0 by default"
  fail=1
}

# Overlay enables mode 3 + FG-off contract
grep -q 'seta r_aaMode 3' "$ROOT/config/vulkan_overlay_present_adaptive_recon.cfg" || {
  echo "FAIL adaptive overlay missing aaMode 3"
  fail=1
}
grep -q 'seta r_temporalWeaponAfterTaa 1' "$ROOT/config/vulkan_overlay_present_adaptive_recon.cfg" || {
  echo "FAIL adaptive overlay must pin weapon-after"
  fail=1
}
grep -q 'seta r_hybrid1 0' "$ROOT/config/vulkan_overlay_present_adaptive_recon.cfg" || {
  echo "FAIL adaptive overlay must lock RT/Hybrid1 off"
  fail=1
}

# Certified boot must not force adaptive recon
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*seta[[:space:]]+r_aaMode[[:space:]]+3([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not force aaMode 3"
      fail=1
    else
      echo "OK  $cfg does not force aaMode 3"
    fi
  fi
done

# Mode 3 must not run full-frame SMAA with temporal (double-blur)
if grep -A20 'case 3:' "$ROOT/renderers/vulkan/vk_aa_policy.c" | grep -q 'r_ext_smaa.*, 1'; then
  echo "FAIL mode 3 must not enable full-frame SMAA with temporal"
  fail=1
else
  echo "OK  mode 3 does not enable full-frame SMAA"
fi

# aaMode 3 included in temporal reconstruction wanted
grep -q 'r_aaMode->integer >= 3' "$ROOT/renderers/vulkan/vk_temporal.c" || {
  echo "FAIL temporal_reconstruction_wanted must include aaMode 3"
  fail=1
}

if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays' \
  "$ROOT/renderers/vulkan/vk_present_recon.c"; then
  echo "FAIL present_recon must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in present_recon"
fi
if ! grep -q 'frame_generation    : off' "$ROOT/renderers/vulkan/vk_present_recon.c"; then
  echo "FAIL present_recon must report frame_generation=off"
  fail=1
else
  echo "OK  frame_generation reported off"
fi

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_5_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_5_check: PASS (static)"
exit 0
