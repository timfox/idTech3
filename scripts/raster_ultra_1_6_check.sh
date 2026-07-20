#!/usr/bin/env bash
# Static gate: Raster Ultra 1.6 GPU scene / meshlets / Hi-Z.
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

need "renderers/vulkan/vk_gpu_scene.c"
need "renderers/vulkan/vk_gpu_scene.h"
need "renderers/vulkan/vk_hiz.c"
need "renderers/vulkan/vk_hiz.h"
need "docs/RASTER_ULTRA_1.6.md"
need "config/vulkan_overlay_raster_ultra_1_6_geometry.cfg"
need "config/modern_raster_ultra.cfg"

grep -q 'vk_gpu_scene_cull_and_build_indirect' "$ROOT/renderers/vulkan/tr_backend.c" || {
  echo "FAIL backend missing GPU scene cull hook"
  fail=1
}
grep -q 'vk_gpu_scene_on_world_load' "$ROOT/renderers/vulkan/tr_bsp.c" || {
  echo "FAIL world load missing gpu_scene invalidate"
  fail=1
}
grep -q 'vk_gpu_scene_init' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL tr_init missing gpu_scene_init"
  fail=1
}
grep -q 'keeping entities visible' "$ROOT/renderers/vulkan/vk_occlusion.c" || {
  echo "FAIL occlusion readback must keep entities visible on failure"
  fail=1
}
grep -q 'R_Meshlets_StableKey' "$ROOT/renderers/vulkan/vk_meshlets.c" || {
  echo "FAIL meshlets missing stable key API"
  fail=1
}
grep -q 'coneCutoff' "$ROOT/renderers/vulkan/vk_meshlets.h" || {
  echo "FAIL meshlets missing cone metadata"
  fail=1
}
grep -q 'VK_WORLD_TYPE_CLASSIC_BSP' "$ROOT/renderers/vulkan/vk_gpu_scene.h" || {
  echo "FAIL missing classic BSP world type"
  fail=1
}
grep -q 'r_forwardPlusHiZ is tile probe pad' "$ROOT/renderers/vulkan/vk_hiz.c" || {
  echo "FAIL HiZ must document distinction from forwardPlusHiZ"
  fail=1
}

# Boot / Ultra must not force GPU scene
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg modern_raster_ultra.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*seta[[:space:]]+r_gpuScene[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not force r_gpuScene 1"
      fail=1
    else
      echo "OK  $cfg does not force r_gpuScene"
    fi
  fi
done

grep -q 'seta r_gpuScene 1' "$ROOT/config/vulkan_overlay_raster_ultra_1_6_geometry.cfg" || {
  echo "FAIL geometry overlay missing r_gpuScene 1"
  fail=1
}
grep -q 'seta r_gpuSceneWorldType 0' "$ROOT/config/vulkan_overlay_raster_ultra_1_6_geometry.cfg" || {
  echo "FAIL overlay must default classic BSP world type"
  fail=1
}
grep -q 'seta r_hybrid1 0' "$ROOT/config/vulkan_overlay_raster_ultra_1_6_geometry.cfg" || {
  echo "FAIL overlay must lock RT off"
  fail=1
}

if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays' \
  "$ROOT/renderers/vulkan/vk_gpu_scene.c" \
  "$ROOT/renderers/vulkan/vk_hiz.c"; then
  echo "FAIL GPU scene/HiZ must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in gpu_scene/hiz"
fi

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_6_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_6_check: PASS (static)"
exit 0
