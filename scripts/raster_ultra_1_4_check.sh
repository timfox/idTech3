#!/usr/bin/env bash
# Static gate: Raster Ultra 1.4 transparency / particles / decals / distortion.
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

need "renderers/vulkan/vk_transparency_route.c"
need "renderers/vulkan/vk_transparency_route.h"
need "renderers/vulkan/vk_gpu_particles.c"
need "renderers/vulkan/vk_deferred_decals.c"
need "renderers/vulkan/vk_distortion.c"
need "renderers/vulkan/vk_raster_fx_spirv.inc"
need "renderers/vulkan/shaders/glsl/raster_fx/gp_update.comp"
need "renderers/vulkan/shaders/glsl/raster_fx/gp_soft_splat.comp"
need "renderers/vulkan/shaders/glsl/raster_fx/dd_apply.comp"
need "renderers/vulkan/shaders/glsl/raster_fx/distortion_apply.comp"
need "docs/RASTER_ULTRA_1.4.md"
need "config/modern_raster_ultra.cfg"

grep -q 'r_oit 1' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL ultra WBOIT"; fail=1; }
grep -q 'r_oitClassify 1' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL ultra classify"; fail=1; }
grep -q 'r_refractiveExcludeOit 1' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL refractive exclude"; fail=1; }
grep -q 'r_gpuParticles 1' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL gpu particles"; fail=1; }
grep -q 'r_deferredDecals 1' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL deferred decals"; fail=1; }
grep -q 'r_distortion 1' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL distortion"; fail=1; }
grep -q 'r_hybrid1 0' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL RT lock"; fail=1; }
grep -q 'r_taa 0' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL TAA off"; fail=1; }

grep -q 'vk_transparency_is_refractive' "$ROOT/renderers/vulkan/tr_backend.c" || { echo "FAIL refractive filter"; fail=1; }
grep -q 'RB_DrawRefractiveAfterOit' "$ROOT/renderers/vulkan/tr_backend.c" || { echo "FAIL refractive after OIT"; fail=1; }
grep -q 'vk_deferred_decals_apply_to_gbuffer' "$ROOT/renderers/vulkan/tr_backend.c" || { echo "FAIL decal hook"; fail=1; }
grep -q 'vk_gpu_particles_apply_after_geometry' "$ROOT/renderers/vulkan/tr_backend.c" || { echo "FAIL particle hook"; fail=1; }
grep -q 'vk_distortion_apply' "$ROOT/renderers/vulkan/tr_backend.c" || { echo "FAIL distortion hook"; fail=1; }
grep -q 'VK_FORMAT_R16_SFLOAT' "$ROOT/renderers/vulkan/vk_pass_registry.c" || { echo "FAIL spine reveal format"; fail=1; }
grep -q 'r_reactiveMaskForce\|r_rasterUltra' "$ROOT/renderers/vulkan/vk_attachments.c" || { echo "FAIL reactive Ultra alloc"; fail=1; }
grep -q 'rfx_collect\|raster_fx' "$ROOT/scripts/compile_shaders.sh" || { echo "FAIL shader collect"; fail=1; }

# Certified boot must not force Ultra effects
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    # Match only the cvar assignment forms; ignore comments mentioning seta r_oit 1.
    if grep -E '^[[:space:]]*seta[[:space:]]+r_oit[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1 ||
       grep -E '^[[:space:]]*seta[[:space:]]+r_gpuParticles[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1 ||
       grep -E '^[[:space:]]*seta[[:space:]]+r_distortion[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not force Ultra FX"
      fail=1
    else
      echo "OK  $cfg does not force Ultra FX"
    fi
  fi
done

if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays' \
  "$ROOT/renderers/vulkan/vk_gpu_particles.c" \
  "$ROOT/renderers/vulkan/vk_deferred_decals.c" \
  "$ROOT/renderers/vulkan/vk_distortion.c" \
  "$ROOT/renderers/vulkan/vk_transparency_route.c"; then
  echo "FAIL FX modules must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in FX modules"
fi

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_4_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_4_check: PASS (static)"
exit 0
