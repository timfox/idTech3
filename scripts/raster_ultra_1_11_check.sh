#!/usr/bin/env bash
# Static gate: Raster Ultra 1.11 Reference Lab (no new rendering techniques).
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

need "renderers/vulkan/vk_reference_lab.c"
need "renderers/vulkan/vk_reference_lab.h"
need "docs/RASTER_ULTRA_1.11.md"
need "config/vulkan_overlay_raster_ultra_1_11_reference_lab.cfg"
need "scripts/raster_ultra_lab/run_all.sh"
need "scripts/raster_ultra_lab/lab_lifecycle_matrix.sh"
need "scripts/raster_ultra_lab/lab_combination_matrix.sh"
need "scripts/raster_ultra_lab/lab_temporal_sequences.sh"
need "scripts/raster_ultra_lab/lab_report.sh"
need "scripts/raster_ultra_lab/metrics/compare_frame.py"
need "scripts/raster_ultra_lab/metrics/detect_artifacts.py"
need "scripts/raster_ultra_lab/baselines/thresholds.json"
need "scripts/raster_ultra_lab/baselines/gpu_classes.json"

grep -q 'VK_REFLAB_SCENE_COUNT' "$ROOT/renderers/vulkan/vk_reference_lab.h" || {
  echo "FAIL scene catalog missing"
  fail=1
}
grep -q 'no new rendering\|No new rendering\|NO new rendering' "$ROOT/docs/RASTER_ULTRA_1.11.md" || {
  echo "FAIL doc must state no new rendering techniques"
  fail=1
}
grep -q 'vk_reference_lab_begin_frame' "$ROOT/renderers/vulkan/vk_frame_submit.c" || {
  echo "FAIL lab not wired into frame loop"
  fail=1
}
grep -q 'vk_reference_lab_init' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL lab init missing"
  fail=1
}
grep -q 'deterministic' "$ROOT/renderers/vulkan/vk_reference_lab.c" || {
  echo "FAIL lab must support deterministic mode"
  fail=1
}
grep -q 'RMSE\|PSNR\|SSIM' "$ROOT/scripts/raster_ultra_lab/metrics/compare_frame.py" || {
  echo "FAIL metrics must include RMSE/PSNR/SSIM"
  fail=1
}
grep -q 'black_frame\|solid_color' "$ROOT/scripts/raster_ultra_lab/metrics/detect_artifacts.py" || {
  echo "FAIL artifact detector missing core cases"
  fail=1
}

# Boot / Ultra must not force lab
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg modern_raster_ultra.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*seta[[:space:]]+r_referenceLab[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not force r_referenceLab 1"
      fail=1
    else
      echo "OK  $cfg does not force r_referenceLab"
    fi
  fi
done

grep -q 'seta r_referenceLab 1' "$ROOT/config/vulkan_overlay_raster_ultra_1_11_reference_lab.cfg" || {
  echo "FAIL overlay missing r_referenceLab 1"
  fail=1
}
grep -q 'seta r_hybrid1 0' "$ROOT/config/vulkan_overlay_raster_ultra_1_11_reference_lab.cfg" || {
  echo "FAIL overlay must lock RT off"
  fail=1
}
grep -q 'seta r_filmGrain 0' "$ROOT/config/vulkan_overlay_raster_ultra_1_11_reference_lab.cfg" || {
  echo "FAIL overlay must pin grain off"
  fail=1
}

# Must not introduce RT in lab module
if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays|\bBLAS\b|\bTLAS\b' \
  "$ROOT/renderers/vulkan/vk_reference_lab.c"; then
  echo "FAIL reference lab must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in reference lab"
fi

# Lab must not claim new shading techniques in source
if grep -qiE 'path.?trac|ray.?march.*new|new BRDF|new GI technique' \
  "$ROOT/renderers/vulkan/vk_reference_lab.c"; then
  echo "FAIL lab module must not add rendering techniques"
  fail=1
else
  echo "OK  lab module is validation-only"
fi

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_11_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_11_check: PASS (static)"
exit 0
