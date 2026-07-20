#!/usr/bin/env bash
# Static gate: Raster Ultra 1.7 atmosphere / weather / clouds.
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

need "renderers/vulkan/vk_sky_owner.c"
need "renderers/vulkan/vk_sky_owner.h"
need "renderers/vulkan/vk_weather.c"
need "renderers/vulkan/vk_weather.h"
need "renderers/vulkan/vk_volumetric_clouds.c"
need "renderers/vulkan/vk_volumetric_clouds.h"
need "docs/RASTER_ULTRA_1.7.md"
need "config/vulkan_overlay_raster_ultra_1_7_atmosphere.cfg"

grep -q 'vk_sky_owner_wants_classic_skybox' "$ROOT/renderers/vulkan/tr_sky.c" || {
  echo "FAIL classic skybox not gated by sky owner"
  fail=1
}
grep -q 'vk_sky_owner_wants_physical_sky' "$ROOT/renderers/vulkan/vk_atmosphere.c" || {
  echo "FAIL atmosphere not gated by sky owner"
  fail=1
}
grep -q 'dual_sky' "$ROOT/renderers/vulkan/vk_sky_owner.c" || {
  echo "FAIL sky owner must document dual_sky forbidden"
  fail=1
}
grep -q 'vk_weather_fog_density_scale' "$ROOT/renderers/vulkan/vk_volumetric_params.c" || {
  echo "FAIL froxel density missing weather scale"
  fail=1
}
grep -q 'vk_volumetric_clouds_sun_shadow_factor' "$ROOT/renderers/vulkan/vk_volumetric_params.c" || {
  echo "FAIL froxel sun missing cloud shadow factor"
  fail=1
}
grep -q 'dedicated history' "$ROOT/renderers/vulkan/vk_volumetric_clouds.c" || {
  echo "FAIL clouds must keep dedicated history policy"
  fail=1
}

# Boot / Ultra must not force physical sky
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg modern_raster_ultra.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*seta[[:space:]]+r_skyOwner[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not force r_skyOwner 1"
      fail=1
    else
      echo "OK  $cfg does not force physical sky"
    fi
  fi
done

grep -q 'seta r_skyOwner 1' "$ROOT/config/vulkan_overlay_raster_ultra_1_7_atmosphere.cfg" || {
  echo "FAIL atmosphere overlay missing r_skyOwner 1"
  fail=1
}
grep -q 'seta r_fog_shadows 1' "$ROOT/config/vulkan_overlay_raster_ultra_1_7_atmosphere.cfg" || {
  echo "FAIL overlay must enable froxel raster shadows"
  fail=1
}
grep -q 'seta r_hybrid1 0' "$ROOT/config/vulkan_overlay_raster_ultra_1_7_atmosphere.cfg" || {
  echo "FAIL overlay must lock RT off"
  fail=1
}

if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays|BLAS|TLAS' \
  "$ROOT/renderers/vulkan/vk_sky_owner.c" \
  "$ROOT/renderers/vulkan/vk_weather.c" \
  "$ROOT/renderers/vulkan/vk_volumetric_clouds.c" \
  "$ROOT/renderers/vulkan/vk_atmosphere.c"; then
  echo "FAIL atmosphere stack must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in atmosphere stack"
fi

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_7_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_7_check: PASS (static)"
exit 0
