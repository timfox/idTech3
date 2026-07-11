#!/usr/bin/env bash
# Wiring test: Vulkan RTX BLAS/TLAS + hybrid frame path scaffolding.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
failures=0

VK_RTX="$(idtech3_file renderers/vulkan/extensions/rtx/vk_rtx.c src/renderers/vulkan/extensions/rtx/vk_rtx.c)"
VK_RTX_WORLD="$(idtech3_file renderers/vulkan/extensions/rtx/vk_rtx_world.c src/renderers/vulkan/extensions/rtx/vk_rtx_world.c)"
VK_RTX_ENT="$(idtech3_file renderers/vulkan/extensions/rtx/vk_rtx_entities.c src/renderers/vulkan/extensions/rtx/vk_rtx_entities.c)"
VK_RTX_ENT_H="$(idtech3_file renderers/vulkan/extensions/rtx/vk_rtx_entities.h src/renderers/vulkan/extensions/rtx/vk_rtx_entities.h)"
VK_RP="$(idtech3_file renderers/vulkan/vk_render_pass.c src/renderers/vulkan/vk_render_pass.c)"
RCHIT="$(idtech3_file renderers/vulkan/shaders/glsl/rtx_demo.rchit src/renderers/vulkan/shaders/glsl/rtx_demo.rchit)"
TR_INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"

check() {
  if ! grep -q "$2" "$1" 2>/dev/null; then
    echo "FAIL: $3"
    failures=$((failures + 1))
  else
    echo "PASS: $3"
  fi
}

check "$VK_RTX" 'r_rtxTlasUpdate' 'TLAS update cvar wiring'
check "$VK_RTX" 'VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR' 'TLAS UPDATE build mode'
check "$VK_RTX" 'ALLOW_UPDATE_BIT_KHR' 'TLAS ALLOW_UPDATE flag'
check "$VK_RTX" 'rtx_status' 'rtx_status console command'
check "$VK_RTX" 'vk_rtx_rebuild_entity_tlas' 'entity TLAS refresh path'
check "$VK_RTX" 'r_rtxEntityTriCap' 'entity triangle budget cvar use'
check "$VK_RTX" 'entity_mesh_count' 'rtx_status mesh/proxy counts'
check "$VK_RTX" 'vk_rtx_bind_world_albedo_ssbo' 'world albedo SSBO bind API'
check "$VK_RTX" 'albedo_buffer' 'world albedo buffer storage'
check "$VK_RTX_WORLD" 'vk_rtx_world_pack' 'world BLAS pack'
check "$VK_RTX_WORLD" 'SF_GRID' 'SF_GRID patch packing'
check "$VK_RTX_WORLD" 'rtx_emit_grid_tris' 'grid triangle emit'
check "$VK_RTX_WORLD" 'albedoRgb' 'per-primitive albedo pack'
check "$VK_RTX_ENT" 'vk_rtx_pack_md3' 'MD3 entity mesh pack'
check "$VK_RTX_ENT" 'vk_rtx_pack_aabb' 'AABB proxy fallback'
check "$VK_RTX_ENT_H" 'vkRtxEntityPackStats_t' 'entity pack stats API'
check "$VK_RP" 'vk_hybrid1_active' 'hybrid frame path priority'
check "$RCHIT" 'gl_InstanceCustomIndexEXT' 'instance-aware closest-hit'
check "$TR_INIT" 'r_rtxTlasUpdate' 'r_rtxTlasUpdate cvar registration'
check "$TR_INIT" 'r_rtxEntityTriCap' 'r_rtxEntityTriCap cvar registration'

HIT_GLSL="$(idtech3_file renderers/vulkan/shaders/glsl/hybrid1/hybrid1_hit.glsl src/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_hit.glsl)"
PT_HIT="$(idtech3_file renderers/vulkan/shaders/glsl/pt_hit.rchit src/renderers/vulkan/shaders/glsl/pt_hit.rchit)"
check "$HIT_GLSL" 'WorldAlbedoSSBO' 'Hybrid1 hit world albedo SSBO'
check "$PT_HIT" 'WorldAlbedoSSBO' 'pathtrace hit world albedo SSBO'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All Vulkan RTX wiring checks passed."
