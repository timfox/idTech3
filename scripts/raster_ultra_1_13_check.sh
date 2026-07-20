#!/usr/bin/env bash
# Static gate: Raster Ultra 1.13 radiance clipmap GI (no GPU).
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

need "renderers/vulkan/vk_radiance_clipmap.c"
need "renderers/vulkan/vk_radiance_clipmap.h"
need "renderers/vulkan/shaders/glsl/raster_gi/rgi_clipmap_sample.comp"
need "docs/RASTER_ULTRA_1.13.md"
need "config/vulkan_overlay_raster_ultra_1_13_radiance_cache.cfg"
need "config/modern_raster_ultra.cfg"

grep -q 'r_radianceCache' "$ROOT/renderers/vulkan/vk_radiance_clipmap.c" || { echo "FAIL r_radianceCache cvar"; fail=1; }
grep -q 'vk_radiance_clipmap_update' "$ROOT/renderers/vulkan/vk_raster_gi.c" || { echo "FAIL raster_gi update hook"; fail=1; }
grep -q 'vk_radiance_clipmap_dispatch_sample' "$ROOT/renderers/vulkan/vk_raster_gi.c" || { echo "FAIL raster_gi sample hook"; fail=1; }
grep -q 'cacheIrrTex' "$ROOT/renderers/vulkan/shaders/glsl/raster_gi/rgi_resolve.comp" || { echo "FAIL resolve cache binding"; fail=1; }
grep -q 'VK_SPINE_RES_RADIANCE_CLIPMAP' "$ROOT/renderers/vulkan/vk_pass_registry.h" || { echo "FAIL spine resource"; fail=1; }
grep -q 'rgi_clipmap_sample' "$ROOT/scripts/compile_shaders.sh" || { echo "FAIL shader collect"; fail=1; }
grep -q 'emissiveAffectsGI' "$ROOT/renderers/vulkan/tr_shader.c" || { echo "FAIL emissive GI keyword"; fail=1; }
grep -q 'r_radianceCache 1' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL ultra enables radiance cache"; fail=1; }

# Certified boot must not enable radiance cache
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -qE 'r_radianceCache[[:space:]]+1' "$ROOT/config/$cfg"; then
      echo "FAIL $cfg must not force r_radianceCache 1"
      fail=1
    else
      echo "OK  $cfg does not force radiance cache"
    fi
  fi
done

# RT lock in overlay + ultra
grep -q 'seta r_hybrid1 0' "$ROOT/config/vulkan_overlay_raster_ultra_1_13_radiance_cache.cfg" || { echo "FAIL overlay RT lock"; fail=1; }
grep -q 'seta r_rcgi 0' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL ultra rcgi lock"; fail=1; }

# No RT APIs in radiance clipmap
if grep -qiE 'accelerationStructure|rayQuery|BLAS|TLAS|vkCmdTraceRays' \
  "$ROOT/renderers/vulkan/vk_radiance_clipmap.c"; then
  echo "FAIL vk_radiance_clipmap.c must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in vk_radiance_clipmap.c"
fi

# Ownership: lightmap delta / static scale still present
grep -q 'r_probeGiStaticScale 0' "$ROOT/config/modern_raster_ultra.cfg" || {
  echo "FAIL ultra must keep probe static scale 0 (no LM double-count)"
  fail=1
}

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_13_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_13_check: PASS (static)"
exit 0
