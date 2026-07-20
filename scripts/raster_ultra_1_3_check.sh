#!/usr/bin/env bash
# Static gate: Raster Ultra 1.3 probe GI + SSGI (no GPU).
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

need "renderers/vulkan/vk_raster_gi.c"
need "renderers/vulkan/vk_raster_gi.h"
need "renderers/vulkan/vk_raster_gi_spirv.inc"
need "renderers/vulkan/shaders/glsl/raster_gi/rgi_probe_sample.comp"
need "renderers/vulkan/shaders/glsl/raster_gi/rgi_ssgi.comp"
need "renderers/vulkan/shaders/glsl/raster_gi/rgi_resolve.comp"
need "docs/RASTER_ULTRA_1.3.md"
need "config/modern_raster_ultra.cfg"

grep -q 'r_probeGi' "$ROOT/renderers/vulkan/vk_raster_gi.c" || { echo "FAIL probe cvar"; fail=1; }
grep -q 'r_ssgi' "$ROOT/renderers/vulkan/vk_raster_gi.c" || { echo "FAIL ssgi cvar"; fail=1; }
grep -q 'vk_raster_gi_apply_after_geometry' "$ROOT/renderers/vulkan/tr_backend.c" || { echo "FAIL backend hook"; fail=1; }
grep -q 'VK_SPINE_PASS_RASTER_GI' "$ROOT/renderers/vulkan/vk_pass_registry.h" || { echo "FAIL spine pass"; fail=1; }
grep -q 'VK_SPINE_RES_INDIRECT_DIFFUSE' "$ROOT/renderers/vulkan/vk_pass_registry.h" || { echo "FAIL spine res"; fail=1; }
grep -q 'r_probeGi 1' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL ultra probe enable"; fail=1; }
grep -q 'r_ssgi 1' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL ultra ssgi enable"; fail=1; }
grep -q 'r_hybrid1 0' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL RT lock"; fail=1; }
grep -q 'r_surfelGi 0' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL surfel lock"; fail=1; }
grep -q 'r_rcgi 0' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL rcgi lock"; fail=1; }
grep -q 'rgi_collect' "$ROOT/scripts/compile_shaders.sh" || { echo "FAIL shader collect"; fail=1; }
grep -q 'vk_raster_gi_sample_entity' "$ROOT/renderers/vulkan/tr_light.c" || { echo "FAIL entity probe sample"; fail=1; }

# Certified boot must not enable probe GI / SSGI
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -qE 'r_probeGi[[:space:]]+1|r_ssgi[[:space:]]+1' "$ROOT/config/$cfg"; then
      echo "FAIL $cfg must not force probe/SSGI on"
      fail=1
    else
      echo "OK  $cfg does not force probe/SSGI"
    fi
  fi
done

# No RT dependency in raster GI module
if grep -qiE 'accelerationStructure|rayQuery|BLAS|TLAS|vkCmdTraceRays' "$ROOT/renderers/vulkan/vk_raster_gi.c"; then
  echo "FAIL vk_raster_gi.c must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in vk_raster_gi.c"
fi

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_3_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_3_check: PASS (static)"
exit 0
