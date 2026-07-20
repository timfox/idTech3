#!/usr/bin/env bash
# Static gate: High-Throughput Raster Engine 1.1 — Slice A (skeletal).
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

need "docs/HIGH_THROUGHPUT_RASTER_1.1.md"
need "config/modern_high_throughput_animation.cfg"
need "renderers/vulkan/vk_ht_animation.c"
need "renderers/vulkan/vk_ht_animation.h"
need "config/modern_high_throughput.cfg"

CFG="$ROOT/config/modern_high_throughput_animation.cfg"

grep -q 'exec modern_high_throughput.cfg' "$CFG" || { echo "FAIL must build on HT 1.0"; fail=1; }
grep -q 'seta r_htAnimation 1' "$CFG" || { echo "FAIL htAnimation required"; fail=1; }
grep -q 'seta r_iqmGpu 1' "$CFG" || { echo "FAIL r_iqmGpu required"; fail=1; }
grep -q 'seta r_animLod 1' "$CFG" || { echo "FAIL animLod required"; fail=1; }
grep -q 'seta r_animCompress 1' "$CFG" || { echo "FAIL animCompress required"; fail=1; }
grep -q 'seta r_hybrid1 0' "$CFG" || { echo "FAIL RT lock"; fail=1; }
grep -q 'seta r_rtx 0' "$CFG" || { echo "FAIL RT lock"; fail=1; }
grep -q 'seta r_gpuSceneWorldType 0' "$CFG" || { echo "FAIL classic BSP world type required"; fail=1; }

grep -q 'vk_ht_anim_compress_pose_tracks' "$ROOT/renderers/vulkan/vk_ht_animation.c" || {
  echo "FAIL clip compression missing"; fail=1;
}
grep -q 'vk_ht_anim_want_iqm_gpu_skin' "$ROOT/renderers/vulkan/vk_ht_animation.c" || {
  echo "FAIL IQM GPU skin gate missing"; fail=1;
}
grep -q 'vk_ht_anim_want_iqm_gpu_skin' "$ROOT/renderers/vulkan/tr_model_iqm.c" || {
  echo "FAIL IQM draw path not wired to ht-anim"; fail=1;
}
grep -q 'activeCount may be 0 for skin-only' "$ROOT/renderers/vulkan/tr_model_iqm.c" || {
  echo "FAIL IQM commit must allow skin-only GPU path"; fail=1;
}
grep -q 'vk_ht_anim_select_lod' "$ROOT/renderers/vulkan/vk_ht_animation.c" || {
  echo "FAIL anim LOD missing"; fail=1;
}
grep -q 'vk_ht_animation_init' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL tr_init missing ht animation init"; fail=1;
}
grep -q 'vk_ht_animation_begin_frame' "$ROOT/renderers/vulkan/vk_frame_submit.c" || {
  echo "FAIL frame submit missing ht animation begin_frame"; fail=1;
}

# Boot must not force HT animation
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg modern_high_throughput.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*exec[[:space:]]+modern_high_throughput_animation\.cfg' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not exec high_throughput_animation"
      fail=1
    else
      echo "OK  $cfg does not exec high_throughput_animation"
    fi
    if grep -E '^[[:space:]]*seta[[:space:]]+r_htAnimation[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not force r_htAnimation 1"
      fail=1
    else
      echo "OK  $cfg does not force r_htAnimation"
    fi
  fi
done

if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays' \
  "$ROOT/renderers/vulkan/vk_ht_animation.c"; then
  echo "FAIL ht animation module must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in ht animation"
fi

# Slice C must not be claimed as shipped in the 1.1 Slice A cfg
if grep -qiE 'r_geomCache|geometry.cache.*1' "$CFG"; then
  echo "FAIL Slice A cfg must not enable geometry cache"
  fail=1
else
  echo "OK  no geometry-cache enable in Slice A cfg"
fi

grep -qi 'not.*boot default\|NOT the boot default' "$ROOT/docs/HIGH_THROUGHPUT_RASTER_1.1.md" "$CFG" || {
  echo "FAIL must label as non-default candidate"
  fail=1
}

if [[ "$fail" -ne 0 ]]; then
  echo "high_throughput_1_1_check: FAIL"
  exit 1
fi
echo "high_throughput_1_1_check: PASS (static)"
exit 0
