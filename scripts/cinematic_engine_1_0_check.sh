#!/usr/bin/env bash
# Static gate: Cinematic Engine Platform 1.0 — Environment Vertical Slice.
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

need "docs/CINEMATIC_ENGINE_PLATFORM_1.0.md"
need "config/modern_cinematic_raster.cfg"
need "renderers/vulkan/vk_scene_platform.c"
need "renderers/vulkan/vk_scene_platform.h"
need "renderers/vulkan/vk_photometric.c"
need "renderers/vulkan/vk_photometric.h"

CFG="$ROOT/config/modern_cinematic_raster.cfg"

grep -q 'exec modern_raster_cinematic.cfg' "$CFG" || { echo "FAIL must build on raster cinematic"; fail=1; }
grep -q 'seta r_scenePlatform 1' "$CFG" || { echo "FAIL scene platform required"; fail=1; }
grep -q 'seta r_photometricLights 1' "$CFG" || { echo "FAIL photometric contract required"; fail=1; }
grep -q 'seta r_taa 0' "$CFG" || { echo "FAIL TAA must stay off"; fail=1; }
grep -q 'seta r_hybrid1 0' "$CFG" || { echo "FAIL RT lock"; fail=1; }
grep -q 'seta r_rtx 0' "$CFG" || { echo "FAIL RT lock"; fail=1; }
grep -q 'vulkan_overlay_raster_ultra_1_8_materials.cfg' "$CFG" || { echo "FAIL materials overlay"; fail=1; }
grep -q 'vulkan_overlay_raster_ultra_1_10_hdr_presentation.cfg' "$CFG" || { echo "FAIL HDR/camera overlay"; fail=1; }

grep -q 'vk_scene_platform_edit_transform' "$ROOT/renderers/vulkan/vk_scene_platform.c" || {
  echo "FAIL live transform edit missing"; fail=1;
}
grep -q 'vk_gpu_scene_update_instance_transform' "$ROOT/renderers/vulkan/vk_scene_platform.c" || {
  echo "FAIL GPU instance link on edit missing"; fail=1;
}
grep -q 'screenshotEXR' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL screenshotEXR command missing"; fail=1;
}
grep -q 'RB_TakeScreenshotEXR\|R_SaveEXR' "$ROOT/renderers/vulkan/tr_init_capture.inc" || {
  echo "FAIL EXR capture path missing"; fail=1;
}
grep -q 'vk_photometric_kelvin_to_rgb' "$ROOT/renderers/vulkan/vk_photometric.c" || {
  echo "FAIL Kelvin conversion missing"; fail=1;
}
grep -q 'ltc_tables.h' "$ROOT/renderers/vulkan/vk_photometric.c" || {
  echo "FAIL LTC tables must be referenced"; fail=1;
}

# Boot must not force cinematic platform
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*exec[[:space:]]+modern_cinematic_raster\.cfg' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not exec cinematic platform"
      fail=1
    else
      echo "OK  $cfg does not exec cinematic platform"
    fi
  fi
done

if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays' \
  "$ROOT/renderers/vulkan/vk_scene_platform.c" \
  "$ROOT/renderers/vulkan/vk_photometric.c"; then
  echo "FAIL scene/photometric modules must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in scene/photometric"
fi

grep -qi 'not.*boot default\|NOT the boot default' "$ROOT/docs/CINEMATIC_ENGINE_PLATFORM_1.0.md" "$CFG" || {
  echo "FAIL must label as non-default candidate"
  fail=1
}

if [[ "$fail" -ne 0 ]]; then
  echo "cinematic_engine_1_0_check: FAIL"
  exit 1
fi
echo "cinematic_engine_1_0_check: PASS (static)"
exit 0
