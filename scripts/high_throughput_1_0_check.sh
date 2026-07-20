#!/usr/bin/env bash
# Static gate: High-Throughput Raster Engine 1.0 — Slice A.
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

need "docs/HIGH_THROUGHPUT_RASTER_1.0.md"
need "config/modern_high_throughput.cfg"
need "renderers/vulkan/vk_ht_throughput.c"
need "renderers/vulkan/vk_ht_throughput.h"
need "renderers/vulkan/vk_gpu_scene.c"
need "config/vulkan_overlay_raster_ultra_1_6_geometry.cfg"

CFG="$ROOT/config/modern_high_throughput.cfg"

grep -q 'exec modern_raster_ultra.cfg' "$CFG" || { echo "FAIL must build on raster ultra"; fail=1; }
grep -q 'exec vulkan_overlay_raster_ultra_1_6_geometry.cfg' "$CFG" || { echo "FAIL geometry overlay required"; fail=1; }
grep -q 'seta r_htThroughput 1' "$CFG" || { echo "FAIL htThroughput required"; fail=1; }
grep -q 'seta r_htDecalBin 1' "$CFG" || { echo "FAIL decal bin required"; fail=1; }
grep -q 'seta r_htMergeDraws 1' "$CFG" || { echo "FAIL merge required"; fail=1; }
grep -q 'seta r_gpuSceneWorldType 0' "$CFG" || { echo "FAIL classic BSP world type required"; fail=1; }
grep -q 'seta r_hybrid1 0' "$CFG" || { echo "FAIL RT lock"; fail=1; }
grep -q 'seta r_rtx 0' "$CFG" || { echo "FAIL RT lock"; fail=1; }

grep -q 'VK_HT_RES_INVALID' "$ROOT/renderers/vulkan/vk_ht_throughput.h" || {
  echo "FAIL index 0 must be INVALID"; fail=1;
}
grep -q 'vk_ht_res_resolve' "$ROOT/renderers/vulkan/vk_ht_throughput.c" || {
  echo "FAIL resource resolve missing"; fail=1;
}
grep -q 'vk_ht_decal_bin_for_view' "$ROOT/renderers/vulkan/vk_ht_throughput.c" || {
  echo "FAIL decal bin missing"; fail=1;
}
grep -q 'vk_gpu_scene_merge_compatible_draws' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || {
  echo "FAIL geometry merge missing"; fail=1;
}
grep -q 'vk_ht_merge_gpu_scene_draws' "$ROOT/renderers/vulkan/tr_backend.c" || {
  echo "FAIL backend merge hook missing"; fail=1;
}
grep -q 'vk_ht_throughput_init' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL tr_init missing ht init"; fail=1;
}
grep -q 'vk_ht_throughput_end_frame' "$ROOT/renderers/vulkan/vk_frame_submit.c" || {
  echo "FAIL frame submit missing ht end_frame"; fail=1;
}

# Boot must not force high-throughput
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*exec[[:space:]]+modern_high_throughput\.cfg' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not exec high_throughput"
      fail=1
    else
      echo "OK  $cfg does not exec high_throughput"
    fi
    if grep -E '^[[:space:]]*seta[[:space:]]+r_htThroughput[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not force r_htThroughput 1"
      fail=1
    else
      echo "OK  $cfg does not force r_htThroughput"
    fi
  fi
done

if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays' \
  "$ROOT/renderers/vulkan/vk_ht_throughput.c"; then
  echo "FAIL ht module must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in ht throughput"
fi

grep -qi 'not.*boot default\|NOT the boot default' "$ROOT/docs/HIGH_THROUGHPUT_RASTER_1.0.md" "$CFG" || {
  echo "FAIL must label as non-default candidate"
  fail=1
}

if [[ "$fail" -ne 0 ]]; then
  echo "high_throughput_1_0_check: FAIL"
  exit 1
fi
echo "high_throughput_1_0_check: PASS (static)"
exit 0
