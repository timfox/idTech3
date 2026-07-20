#!/usr/bin/env bash
# Static gate: Raster Ultra 1.1 CSM + clearcoat packing (no GPU).
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

need "renderers/vulkan/vk_sun_csm.c"
need "renderers/vulkan/vk_sun_csm.h"
need "docs/RASTER_ULTRA_1.1.md"
need "config/modern_raster_ultra.cfg"

grep -q 'r_sunShadowCascades' "$ROOT/renderers/vulkan/vk_sun_csm.c" || { echo "FAIL CSM cvar"; fail=1; }
grep -q 'RB_BuildSunShadowViewRange' "$ROOT/renderers/vulkan/tr_backend.c" || { echo "FAIL CSM build range"; fail=1; }
grep -q 'pbrSunShadowCascadeRows' "$ROOT/renderers/vulkan/vk.h" || { echo "FAIL UBO cascade rows"; fail=1; }
grep -q 'pbrSunShadowCascadeRows' "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" || { echo "FAIL shader cascade rows"; fail=1; }
grep -q 'r_sunShadowCascades 4' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL ultra cascades"; fail=1; }
grep -q 'r_hybrid1 0' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL RT lock"; fail=1; }
grep -q 'deferredClearcoat\|clearcoat' "$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" || { echo "FAIL deferred clearcoat"; fail=1; }

# Certified boot must not force 4 cascades
if grep -q 'r_sunShadowCascades 4' "$ROOT/config/modern_vulkan_stable.cfg" 2>/dev/null; then
  echo "FAIL stable must not force CSM×4"
  fail=1
else
  echo "OK  stable does not force CSM×4"
fi

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_1_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_1_check: PASS (static)"
exit 0
