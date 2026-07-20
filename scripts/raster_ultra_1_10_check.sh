#!/usr/bin/env bash
# Static gate: Raster Ultra 1.10 HDR presentation / color / cinematic camera.
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

need "renderers/vulkan/vk_present_color.c"
need "renderers/vulkan/vk_present_color.h"
need "renderers/vulkan/vk_exposure_histogram.c"
need "renderers/vulkan/vk_exposure_histogram.h"
need "renderers/vulkan/vk_cinematic_camera.c"
need "renderers/vulkan/vk_cinematic_camera.h"
need "renderers/vulkan/vk_capture_pipeline.c"
need "renderers/vulkan/vk_capture_pipeline.h"
need "renderers/vulkan/vk_color_grade.c"
need "renderers/vulkan/vk_color_grade.h"
need "docs/RASTER_ULTRA_1.10.md"
need "config/vulkan_overlay_raster_ultra_1_10_hdr_presentation.cfg"

grep -q 'vk_present_color_on_surface_formats' "$ROOT/renderers/vulkan/vk_device.c" || {
  echo "FAIL surface format probe missing present_color"
  fail=1
}
grep -q 'vk_present_color_apply_selection' "$ROOT/renderers/vulkan/vk_device.c" || {
  echo "FAIL present_color selection not applied"
  fail=1
}
grep -q 'vk_exposure_histogram_notify_luminance' "$ROOT/renderers/vulkan/vk_temporal.c" || {
  echo "FAIL auto-exposure missing histogram notify"
  fail=1
}
grep -q 'vk_exposure_histogram_on_camera_cut' "$ROOT/renderers/vulkan/vk_temporal.c" || {
  echo "FAIL exposure cut reset missing"
  fail=1
}
grep -q 'vk_capture_pipeline_allow_sdr_encode' "$ROOT/renderers/vulkan/tr_backend.c" || {
  echo "FAIL screenshots missing HDR→SDR guard"
  fail=1
}
grep -q 'vk_cinematic_camera_begin_frame' "$ROOT/renderers/vulkan/vk_frame_submit.c" || {
  echo "FAIL cinematic camera not in frame loop"
  fail=1
}
grep -q 'double gamma\|doubleGamma' "$ROOT/renderers/vulkan/vk_present_color.c" || {
  echo "FAIL present_color must forbid double gamma"
  fail=1
}
grep -q 'UI never\|excludeUI\|HUD never' "$ROOT/renderers/vulkan/vk_cinematic_camera.c" || {
  echo "FAIL cinematic camera must exclude UI"
  fail=1
}
grep -q 'no frame-gen\|frame-gen\|frame generation' "$ROOT/renderers/vulkan/vk_cinematic_camera.c" || {
  echo "FAIL cinematic camera must forbid frame generation"
  fail=1
}

# Boot / Ultra must not force HDR present / cinematic / histogram
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg modern_raster_ultra.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    for cv in r_presentColor r_exposureHistogram r_cinematicCamera; do
      if grep -E "^[[:space:]]*seta[[:space:]]+${cv}[[:space:]]+[1-9]" "$ROOT/config/$cfg" >/dev/null 2>&1; then
        echo "FAIL $cfg must not force $cv"
        fail=1
      else
        echo "OK  $cfg does not force $cv"
      fi
    done
  fi
done

grep -q 'seta r_exposureHistogram 1' "$ROOT/config/vulkan_overlay_raster_ultra_1_10_hdr_presentation.cfg" || {
  echo "FAIL overlay missing r_exposureHistogram 1"
  fail=1
}
grep -q 'seta r_captureBlockHdrToSdr 1' "$ROOT/config/vulkan_overlay_raster_ultra_1_10_hdr_presentation.cfg" || {
  echo "FAIL overlay must block silent HDR→SDR"
  fail=1
}
grep -q 'seta r_localExposure 0' "$ROOT/config/vulkan_overlay_raster_ultra_1_10_hdr_presentation.cfg" || {
  echo "FAIL overlay must keep local exposure off"
  fail=1
}
grep -q 'seta r_hybrid1 0' "$ROOT/config/vulkan_overlay_raster_ultra_1_10_hdr_presentation.cfg" || {
  echo "FAIL overlay must lock RT off"
  fail=1
}

if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays|\bBLAS\b|\bTLAS\b' \
  "$ROOT/renderers/vulkan/vk_present_color.c" \
  "$ROOT/renderers/vulkan/vk_exposure_histogram.c" \
  "$ROOT/renderers/vulkan/vk_cinematic_camera.c" \
  "$ROOT/renderers/vulkan/vk_capture_pipeline.c" \
  "$ROOT/renderers/vulkan/vk_color_grade.c"; then
  echo "FAIL presentation stack must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in presentation stack"
fi

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_10_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_10_check: PASS (static)"
exit 0
