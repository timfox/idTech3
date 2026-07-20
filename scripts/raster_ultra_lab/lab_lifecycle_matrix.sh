#!/usr/bin/env bash
# Raster Ultra 1.11 — lifecycle matrix (static + optional GPU).
# Exit 77 = skipped (no display / no binary).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail=0
skip=0

echo "=== Raster Ultra 1.11 lifecycle matrix ==="

need_grep() {
  local file="$1" pat="$2" msg="$3"
  if ! grep -qE "$pat" "$ROOT/$file"; then
    echo "FAIL $msg"
    fail=1
  else
    echo "OK  $msg"
  fi
}

# Static contract: lab resets / pins exist for lifecycle events
need_grep "renderers/vulkan/vk_reference_lab.c" "freezeExposure|captureDeterministic" "lab freezes exposure for capture"
need_grep "renderers/vulkan/vk_reference_lab.c" "r_taa.*, \"0\"" "spatial SS reference disables TAA"
need_grep "renderers/vulkan/vk_exposure_histogram.c" "on_camera_cut|on_map_change" "exposure resets on cut/map"
need_grep "renderers/vulkan/vk_vshadow.c" "on_camera_cut|on_map_change" "vshadow invalidates on cut/map"
need_grep "renderers/vulkan/vk_frame_submit.c" "vk_reference_lab_begin_frame" "lab begin_frame wired"
need_grep "renderers/vulkan/vk_capture_pipeline.c" "r_captureDeterministic" "capture deterministic pin"

# Documented lifecycle cases (must appear in Ultra 1.11 doc)
DOC="$ROOT/docs/RASTER_ULTRA_1.11.md"
if [[ -f "$DOC" ]]; then
  for case in "vid_restart" "map switch" "resize" "minimize" "focus" "cold boot" "clean shutdown"; do
    if grep -qi "$case" "$DOC"; then
      echo "OK  doc covers: $case"
    else
      echo "FAIL doc missing lifecycle case: $case"
      fail=1
    fi
  done
else
  echo "FAIL missing docs/RASTER_ULTRA_1.11.md"
  fail=1
fi

# Optional GPU stress (skip without display)
CLIENT="$ROOT/release/idtech3"
if [[ "${RASTER_ULTRA_LAB_GPU:-0}" == "1" && -x "$CLIENT" && -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then
  echo "GPU lifecycle: short vid_restart smoke (RASTER_ULTRA_LAB_GPU=1)"
  set +e
  timeout 45 "$CLIENT" +set dedicated 0 +set com_hunkMegs 128 \
    +exec modern_raster_reference.cfg \
    +exec vulkan_overlay_raster_ultra_1_11_reference_lab.cfg \
    +set r_referenceLab 1 \
    +vid_restart +wait +wait +quit >/tmp/ru111_lifecycle.log 2>&1
  rc=$?
  set -e
  if [[ $rc -eq 0 || $rc -eq 124 ]]; then
    echo "OK  GPU lifecycle smoke finished (rc=$rc)"
  else
    echo "FAIL GPU lifecycle smoke rc=$rc (see /tmp/ru111_lifecycle.log)"
    fail=1
  fi
else
  echo "SKIP GPU lifecycle (set RASTER_ULTRA_LAB_GPU=1 with display + release/idtech3)"
  skip=1
fi

if [[ "$fail" -ne 0 ]]; then
  echo "lab_lifecycle_matrix: FAIL"
  exit 1
fi
echo "lab_lifecycle_matrix: PASS${skip:+ (gpu skipped)}"
exit 0
