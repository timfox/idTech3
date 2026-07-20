#!/usr/bin/env bash
# Static gate: Raster Ultra 1.8 material IR / graph / surface evolution.
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

need "renderers/vulkan/vk_material_ir.c"
need "renderers/vulkan/vk_material_ir.h"
need "renderers/vulkan/vk_material_graph.c"
need "renderers/vulkan/vk_material_graph.h"
need "renderers/vulkan/vk_material_instance.c"
need "renderers/vulkan/vk_material_instance.h"
need "renderers/vulkan/vk_material_cache.c"
need "renderers/vulkan/vk_material_cache.h"
need "renderers/vulkan/vk_surface_evolution.c"
need "renderers/vulkan/vk_surface_evolution.h"
need "docs/RASTER_ULTRA_1.8.md"
need "config/vulkan_overlay_raster_ultra_1_8_materials.cfg"

grep -q 'vk_surface_evolution_fill_ubo' "$ROOT/renderers/vulkan/tr_shade.c" || {
  echo "FAIL tr_shade missing surface evolution UBO fill"
  fail=1
}
grep -q 'pbrSurfaceEvolution' "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" || {
  echo "FAIL gen_frag missing pbrSurfaceEvolution"
  fail=1
}
grep -q 'pbrSurfaceEvolution' "$ROOT/renderers/vulkan/vk.h" || {
  echo "FAIL vkUniform_t missing pbrSurfaceEvolution"
  fail=1
}
grep -q 'vk_weather_wetness_rate\|vk_weather_state\|wetnessRate' "$ROOT/renderers/vulkan/vk_surface_evolution.c" || {
  echo "FAIL surface evolution must consume weather wetness"
  fail=1
}
grep -q 'no loops\|topo-ordered\|topo-ordered inputs' "$ROOT/renderers/vulkan/vk_material_graph.c" || {
  echo "FAIL graph must document no-loops / topo policy"
  fail=1
}
grep -q 'runtimeCompiles' "$ROOT/renderers/vulkan/vk_material_cache.c" || {
  echo "FAIL cache must track runtimeCompiles"
  fail=1
}
grep -q 'vk_material_ir_init' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL tr_init missing material IR init"
  fail=1
}
grep -q 'vk_surface_evolution_update' "$ROOT/renderers/vulkan/vk_frame_submit.c" || {
  echo "FAIL frame_submit missing surface evolution update"
  fail=1
}

# Boot / Ultra must not force surface evolution or material IR
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg modern_raster_ultra.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*seta[[:space:]]+r_surfaceEvolution[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not force r_surfaceEvolution 1"
      fail=1
    else
      echo "OK  $cfg does not force r_surfaceEvolution"
    fi
    if grep -E '^[[:space:]]*seta[[:space:]]+r_materialIR[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not force r_materialIR 1"
      fail=1
    else
      echo "OK  $cfg does not force r_materialIR"
    fi
  fi
done

grep -q 'seta r_surfaceEvolution 1' "$ROOT/config/vulkan_overlay_raster_ultra_1_8_materials.cfg" || {
  echo "FAIL materials overlay missing r_surfaceEvolution 1"
  fail=1
}
grep -q 'seta r_materialIR 1' "$ROOT/config/vulkan_overlay_raster_ultra_1_8_materials.cfg" || {
  echo "FAIL materials overlay missing r_materialIR 1"
  fail=1
}
grep -q 'seta r_hybrid1 0' "$ROOT/config/vulkan_overlay_raster_ultra_1_8_materials.cfg" || {
  echo "FAIL overlay must lock RT off"
  fail=1
}

# Evolution must not add new #ifdef FS permutation axes
if grep -E '#ifdef[[:space:]]+USE_SURFACE_EVOLUTION|#if[[:space:]]+defined[[:space:]]*\([[:space:]]*USE_SURFACE_EVOLUTION' \
  "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" >/dev/null 2>&1; then
  echo "FAIL surface evolution must use UBO, not new #ifdef variants"
  fail=1
else
  echo "OK  no USE_SURFACE_EVOLUTION #ifdef permutation"
fi

if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays|BLAS|TLAS' \
  "$ROOT/renderers/vulkan/vk_material_ir.c" \
  "$ROOT/renderers/vulkan/vk_material_graph.c" \
  "$ROOT/renderers/vulkan/vk_material_instance.c" \
  "$ROOT/renderers/vulkan/vk_material_cache.c" \
  "$ROOT/renderers/vulkan/vk_surface_evolution.c"; then
  echo "FAIL materials stack must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in materials stack"
fi

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_8_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_8_check: PASS (static)"
exit 0
