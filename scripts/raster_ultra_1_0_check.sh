#!/usr/bin/env bash
# Static gate: Raster Ultra 1.0 foundation (no GPU).
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

need "renderers/vulkan/vk_raster_ultra.c"
need "renderers/vulkan/vk_raster_ultra.h"
need "config/modern_raster_ultra.cfg"
need "config/modern_low_latency.cfg"
need "config/modern_raster_reference.cfg"
need "config/modern_experimental.cfg"
need "docs/RASTER_ULTRA_1.0.md"

grep -q 'r_rasterUltra' "$ROOT/renderers/vulkan/vk_raster_ultra.c" || { echo "FAIL r_rasterUltra missing"; fail=1; }
grep -q 'VK_RasterUltra_Enforce' "$ROOT/renderers/vulkan/tr_init.c" || { echo "FAIL Enforce not wired"; fail=1; }
grep -q 'VK_RasterUltra_PrintStatus' "$ROOT/renderers/vulkan/tr_init_diagnostics.inc" || { echo "FAIL status line missing"; fail=1; }
grep -q 'rasterUltra:' "$ROOT/renderers/vulkan/vk_raster_ultra.c" || { echo "FAIL PrintStatus format missing"; fail=1; }
grep -q 'ApplyDeferredSpecularAA' "$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" || { echo "FAIL deferred specular AA missing"; fail=1; }
grep -q 'specularAA' "$ROOT/renderers/vulkan/vk_deferred_gbuffer.c" || { echo "FAIL deferred push specularAA missing"; fail=1; }

# Profiles must lock RT
for cfg in modern_raster_ultra.cfg modern_low_latency.cfg modern_raster_reference.cfg modern_clustered.cfg; do
  grep -q 'r_hybrid1 0' "$ROOT/config/$cfg" || { echo "FAIL $cfg missing r_hybrid1 0"; fail=1; }
  grep -q 'r_rtx 0' "$ROOT/config/$cfg" || { echo "FAIL $cfg missing r_rtx 0"; fail=1; }
done

# Certified stable must remain mode 2 and not set r_rasterUltra 1
grep -q 'r_renderMode 2' "$ROOT/config/modern_vulkan_stable.cfg" || { echo "FAIL stable mode not 2"; fail=1; }
if grep -q 'r_rasterUltra 1' "$ROOT/config/modern_vulkan.cfg" "$ROOT/config/modern_vulkan_stable.cfg" 2>/dev/null; then
  echo "FAIL certified profile must not enable r_rasterUltra"
  fail=1
else
  echo "OK  certified profiles do not enable r_rasterUltra"
fi

# Ultra profile enables contract + mode 3 path
grep -q 'r_rasterUltra 1' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL ultra missing r_rasterUltra 1"; fail=1; }
grep -q 'modern_clustered.cfg' "$ROOT/config/modern_raster_ultra.cfg" || { echo "FAIL ultra must exec clustered"; fail=1; }

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_0_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_0_check: PASS (static)"
exit 0
