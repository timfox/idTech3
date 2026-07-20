#!/usr/bin/env bash
# Static gate: Raster Ultra 1.14 Terrain + Vegetation + Biomes.
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

need "renderers/vulkan/vk_terrain.c"
need "renderers/vulkan/vk_terrain.h"
need "renderers/vulkan/vk_biome.c"
need "renderers/vulkan/vk_biome.h"
need "renderers/vulkan/vk_vegetation_gpu.c"
need "renderers/vulkan/vk_vegetation_gpu.h"
need "docs/RASTER_ULTRA_1.14.md"
need "config/vulkan_overlay_raster_ultra_1_14_terrain.cfg"

grep -q 'VK_WORLD_TYPE_TERRAIN' "$ROOT/renderers/vulkan/vk_gpu_scene.h" || {
  echo "FAIL WORLD_TERRAIN missing from vk_gpu_scene.h"
  fail=1
}
grep -q 'WORLD_CLASSIC_BSP\|VK_WORLD_TYPE_CLASSIC_BSP' "$ROOT/renderers/vulkan/vk_gpu_scene.h" || {
  echo "FAIL classic BSP world type missing"
  fail=1
}
grep -q 'CBTerrain_HasMetadata' "$ROOT/renderers/vulkan/vk_terrain.c" || {
  echo "FAIL terrain metadata gate missing"
  fail=1
}
grep -q 'CBTerrain_SampleHeightUV\|s_heightSamples' "$ROOT/renderers/vulkan/vk_terrain.c" || {
  echo "FAIL heightmap CPU sampling missing"
  fail=1
}
grep -q 'CBTerrain_UpdateLOD' "$ROOT/renderers/vulkan/vk_terrain.c" || {
  echo "FAIL terrain LOD update missing"
  fail=1
}
grep -q 'VK_Biome_Evaluate' "$ROOT/renderers/vulkan/vk_biome.c" || {
  echo "FAIL biome evaluate missing"
  fail=1
}
grep -q 'r_biomeSeed' "$ROOT/renderers/vulkan/vk_biome.c" || {
  echo "FAIL deterministic biome seed missing"
  fail=1
}
grep -q 'Veg_Generate' "$ROOT/renderers/vulkan/vk_vegetation_gpu.c" || {
  echo "FAIL vegetation generation missing"
  fail=1
}
grep -q 'VK_VegGpu_Init' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL veg init not wired"
  fail=1
}
grep -q 'VK_Biome_Init' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL biome init not wired"
  fail=1
}
grep -q 'VK_VegGpu_Frame' "$ROOT/renderers/vulkan/tr_backend.c" || {
  echo "FAIL veg frame not wired"
  fail=1
}
grep -q 'CBTerrain_OnWorldLoad' "$ROOT/renderers/vulkan/tr_bsp.c" || {
  echo "FAIL terrain world-load lifecycle missing"
  fail=1
}
grep -q 'VK_SPINE_PASS_TERRAIN_LOD' "$ROOT/renderers/vulkan/vk_pass_registry.h" || {
  echo "FAIL spine terrain passes missing"
  fail=1
}
grep -q 'tiled heightfield\|Tiled heightfield' "$ROOT/docs/RASTER_ULTRA_1.14.md" || {
  echo "FAIL doc must state chosen terrain representation"
  fail=1
}
grep -qiE 'not required|Does NOT|does not.*require' "$ROOT/docs/RASTER_ULTRA_1.14.md" || {
  echo "FAIL doc must state TAA is not required"
  fail=1
}

# Boot profiles must not force terrain on
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*seta[[:space:]]+r_cbtTerrain[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not force r_cbtTerrain 1"
      fail=1
    else
      echo "OK  $cfg does not force r_cbtTerrain"
    fi
    if grep -E '^[[:space:]]*seta[[:space:]]+r_vegGpu[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not force r_vegGpu 1"
      fail=1
    else
      echo "OK  $cfg does not force r_vegGpu"
    fi
  fi
done

# Ultra base must not auto-enable terrain (overlay-only)
if grep -E '^[[:space:]]*seta[[:space:]]+r_cbtTerrain[[:space:]]+1([^0-9]|$)' "$ROOT/config/modern_raster_ultra.cfg" >/dev/null 2>&1; then
  echo "FAIL modern_raster_ultra.cfg must not force r_cbtTerrain 1"
  fail=1
else
  echo "OK  modern_raster_ultra.cfg does not force terrain"
fi

OV="$ROOT/config/vulkan_overlay_raster_ultra_1_14_terrain.cfg"
grep -q 'seta r_cbtTerrain 1' "$OV" || { echo "FAIL overlay missing r_cbtTerrain 1"; fail=1; }
grep -q 'seta r_biome 1' "$OV" || { echo "FAIL overlay missing r_biome 1"; fail=1; }
grep -q 'seta r_vegGpu 1' "$OV" || { echo "FAIL overlay missing r_vegGpu 1"; fail=1; }
grep -q 'seta r_hybrid1 0' "$OV" || { echo "FAIL overlay must lock RT off"; fail=1; }
grep -q 'seta r_rtx 0' "$OV" || { echo "FAIL overlay must lock r_rtx 0"; fail=1; }
grep -q 'seta r_taa 0' "$OV" || { echo "FAIL overlay must not force TAA"; fail=1; }
grep -q 'seta r_aaMode 2' "$OV" || { echo "FAIL overlay should keep SMAA"; fail=1; }
grep -q 'seta r_gpuSceneWorldType 0' "$OV" || { echo "FAIL overlay should keep classic world type default"; fail=1; }

# No RT APIs in new modules
for f in vk_terrain.c vk_biome.c vk_vegetation_gpu.c; do
  if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays|\bBLAS\b|\bTLAS\b' \
    "$ROOT/renderers/vulkan/$f"; then
    echo "FAIL $f must not use RT APIs"
    fail=1
  else
    echo "OK  no RT APIs in $f"
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_14_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_14_check: PASS (static)"
exit 0
