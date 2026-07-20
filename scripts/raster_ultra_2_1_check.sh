#!/usr/bin/env bash
# Static gate: Raster Ultra 2.1 Slice A — spatial-first cinematic AA.
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

need "docs/RASTER_ULTRA_2.1.md"
need "config/modern_raster_cinematic.cfg"
need "renderers/vulkan/vk_spatial_aa.c"
need "renderers/vulkan/vk_spatial_aa.h"
need "renderers/vulkan/shaders/glsl/spatial_adaptive_ss.frag"

CFG="$ROOT/config/modern_raster_cinematic.cfg"

grep -q 'seta r_spatialAa 1' "$CFG" || { echo "FAIL cinematic missing r_spatialAa 1"; fail=1; }
grep -q 'seta r_aaMode 2' "$CFG" || { echo "FAIL cinematic must use SMAA"; fail=1; }
grep -q 'seta r_taa 0' "$CFG" || { echo "FAIL cinematic must keep TAA off"; fail=1; }
grep -q 'seta r_frequencyAware 1' "$CFG" || { echo "FAIL cinematic must enable frequency-aware"; fail=1; }
grep -q 'seta r_frequencySelectiveSS 1' "$CFG" || { echo "FAIL cinematic must enable selective SS responses"; fail=1; }
grep -q 'seta r_hybrid1 0' "$CFG" || { echo "FAIL cinematic RT lock"; fail=1; }
grep -q 'seta r_rtx 0' "$CFG" || { echo "FAIL cinematic RT lock"; fail=1; }
grep -q 'exec modern_raster_ultra_2.cfg' "$CFG" || { echo "FAIL cinematic should build on ultra_2"; fail=1; }

if grep -E '^[[:space:]]*seta[[:space:]]+r_taa[[:space:]]+1([^0-9]|$)' "$CFG" >/dev/null 2>&1; then
  echo "FAIL cinematic must not force TAA on"
  fail=1
fi
if grep -E '^[[:space:]]*seta[[:space:]]+r_aaMode[[:space:]]+[345]([^0-9]|$)' "$CFG" >/dev/null 2>&1; then
  echo "FAIL cinematic must not use temporal aaMode 3–5"
  fail=1
fi

grep -q 'vk_spatial_aa_prepare_input' "$ROOT/renderers/vulkan/vk_post_aa.c" || {
  echo "FAIL adaptive SS not wired into post AA"
  fail=1
}
grep -q 'vk_spatial_aa_init' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL spatial AA init missing"
  fail=1
}
grep -q 'vk_spatial_aa_begin_frame' "$ROOT/renderers/vulkan/vk_frame_submit.c" || {
  echo "FAIL spatial AA begin_frame missing"
  fail=1
}
grep -q 'VK_SPINE_PASS_SPATIAL_AA' "$ROOT/renderers/vulkan/vk_pass_registry.h" || {
  echo "FAIL spine spatial AA pass missing"
  fail=1
}
grep -q 'spatial_adaptive_pipeline' "$ROOT/renderers/vulkan/vk_post_process_pipeline.c" || {
  echo "FAIL spatial adaptive pipeline case missing"
  fail=1
}
grep -q 'history-free\|History-free\|history free' "$ROOT/renderers/vulkan/shaders/glsl/spatial_adaptive_ss.frag" || {
  echo "FAIL adaptive shader must declare history-free"
  fail=1
}

# Boot must not force cinematic
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*exec[[:space:]]+modern_raster_cinematic\.cfg' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not exec cinematic"
      fail=1
    else
      echo "OK  $cfg does not exec cinematic"
    fi
  fi
done

# No RT APIs in spatial AA module
if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays|BLAS|TLAS' \
  "$ROOT/renderers/vulkan/vk_spatial_aa.c"; then
  echo "FAIL vk_spatial_aa.c must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in vk_spatial_aa.c"
fi

grep -q 'Slice A\|spatial-first\|Spatial-First' "$ROOT/docs/RASTER_ULTRA_2.1.md" || {
  echo "FAIL doc must describe Slice A"
  fail=1
}
grep -q 'not.*boot default\|NOT the boot default\|Not the boot default' "$ROOT/docs/RASTER_ULTRA_2.1.md" "$CFG" || {
  echo "FAIL must label cinematic as non-default candidate"
  fail=1
}

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_2_1_check: FAIL"
  exit 1
fi
echo "raster_ultra_2_1_check: PASS (static)"
exit 0
