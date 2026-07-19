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
check "$VK_RTX" 'vk_rtx_state_string' 'rtx_status reports readiness state'
check "$VK_RTX" 'idle: enable r_rtxDemo, r_rtx, r_hybrid1, or r_raygun before vid_restart' 'rtx_status explains disabled consumers'
check "$VK_RTX" 'waiting: TLAS not built yet' 'rtx_status explains pending TLAS'
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
check "$VK_RTX_ENT" 'R_IQMSkinPositions' 'jointed IQM CPU-skin into entity BLAS'
check "$VK_RTX_ENT" 'needSkin = (qboolean)( data->num_joints > 0 )' 'jointed IQM never uses bind-pose'
check "$VK_RTX_ENT_H" 'meshCpuSkinnedCount' 'entity pack CPU-skin success count'
check "$VK_RTX_ENT" 'proxySkinnedCount = proxyIqmFail' 'proxySkinned rollup of skinned-format AABB fails'
check "$VK_RTX" 'proxy_rate=' 'rtx_status reports entity AABB proxy rate'
check "$VK_RTX" 'cpuskin=' 'rtx_status reports CPU-skinned mesh packs'
check "$VK_RP" 'vk_hybrid1_active' 'hybrid frame path priority'
check "$RCHIT" 'gl_InstanceCustomIndexEXT' 'instance-aware closest-hit'
check "$TR_INIT" 'r_rtxTlasUpdate' 'r_rtxTlasUpdate cvar registration'
check "$TR_INIT" 'r_rtxEntityTriCap' 'r_rtxEntityTriCap cvar registration'
check "$TR_INIT" 'r_hybrid1Quality' 'r_hybrid1Quality cvar registration'
check "$TR_INIT" 'Cvar_Get( "r_hybrid1", "0"' 'r_hybrid1 off by default'
check "$TR_INIT" 'Cvar_Get( "r_rtx", "0"' 'r_rtx off by default'

TR_IMAGE="$(idtech3_file renderers/vulkan/tr_image.c src/renderers/vulkan/tr_image.c)"
VK_TEX="$(idtech3_file renderers/vulkan/vk_texture_image.c src/renderers/vulkan/vk_texture_image.c)"
VK_MAT="$(idtech3_file renderers/vulkan/extensions/rtx/vk_rtx_material.c src/renderers/vulkan/extensions/rtx/vk_rtx_material.c)"
check "$TR_IMAGE" 'R_EnsureImageThumb' 'lazy RTX image thumb ensure'
check "$TR_IMAGE" 'R_BuildImageThumbFromPic' 'CPU thumb build from upload pixels'
check "$VK_TEX" 'vk_build_image_thumb_from_gpu' 'GPU blit readback for image thumbs'
check "$VK_MAT" 'R_EnsureImageThumb' 'material path ensures thumbs before UV sample'

HIT_GLSL="$(idtech3_file renderers/vulkan/shaders/glsl/hybrid1/hybrid1_hit.glsl src/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_hit.glsl)"
PT_HIT="$(idtech3_file renderers/vulkan/shaders/glsl/pt_hit.rchit src/renderers/vulkan/shaders/glsl/pt_hit.rchit)"
VK_BINDLESS="$(idtech3_file renderers/vulkan/extensions/rtx/vk_rtx_bindless.c src/renderers/vulkan/extensions/rtx/vk_rtx_bindless.c)"
VK_HYBRID="$(idtech3_file renderers/vulkan/extensions/rtx/vk_hybrid1.c src/renderers/vulkan/extensions/rtx/vk_hybrid1.c)"
VK_WORLD="$(idtech3_file renderers/vulkan/extensions/rtx/vk_rtx_world.c src/renderers/vulkan/extensions/rtx/vk_rtx_world.c)"
VK_ENT="$(idtech3_file renderers/vulkan/extensions/rtx/vk_rtx_entities.c src/renderers/vulkan/extensions/rtx/vk_rtx_entities.c)"
check "$VK_HYBRID" 'HYBRID1_ApplyQualityPreset' 'Hybrid1 quality preset applicator'
check "$VK_HYBRID" 'r_hybrid1->integer <= 0' 'quality presets gated on r_hybrid1'
check "$VK_HYBRID" 'quality=%d(%s)' 'hybrid1_status prints quality tier'
check "$HIT_GLSL" 'WorldAlbedoSSBO' 'Hybrid1 hit world albedo SSBO'
check "$HIT_GLSL" 'PrimMaterialSSBO' 'Hybrid1 hit prim material SSBO'
check "$HIT_GLSL" 'PrimUvSSBO' 'Hybrid1 hit PrimUv SSBO'
check "$HIT_GLSL" 'baryCoord' 'Hybrid1 hit barycentrics'
check "$HIT_GLSL" 'binding = 15' 'Hybrid1 hit bindless diffuse binding'
check "$HIT_GLSL" 'binding = 17' 'Hybrid1 hit PrimUv binding'
check "$HIT_GLSL" 'nonuniformEXT' 'Hybrid1 Phase A.1b nonuniform sample'
check "$HIT_GLSL" 'bindlessMeta' 'Hybrid1 UBO bindless gate'
check "$PT_HIT" 'WorldAlbedoSSBO' 'pathtrace hit world albedo SSBO'
check "$PT_HIT" 'PrimUvSSBO' 'pathtrace PrimUv SSBO'
check "$VK_BINDLESS" 'vk_rtx_bindless_init' 'bindless module init'
check "$VK_BINDLESS" 'bindless=textures:' 'rtx_status bindless line'
check "$VK_BINDLESS" 'set_prim_from_shader' 'Phase A.1 prim textureIndex emit'
check "$VK_BINDLESS" 'validPrims:' 'rtx_status valid prim count'
check "$VK_BINDLESS" 'Phase A.1b' 'Phase A.1b active sampling'
check "$VK_WORLD" 'vk_rtx_bindless_set_prim_from_shader' 'world pack writes prim materials'
check "$VK_WORLD" 'rtx_store_uv6' 'world pack writes PrimUv'
check "$VK_ENT" 'vk_rtx_bindless_set_entity_prim_from' 'entity pack writes prim materials'
check "$VK_HYBRID" 'binding = 15' 'Hybrid1 RT DSL binding 15'
check "$VK_HYBRID" 'binding = 17' 'Hybrid1 RT DSL PrimUv binding'
check "$VK_HYBRID" 'bindless_array_count' 'Hybrid1 bindless array expansion'
check "$VK_HYBRID" 'vk_rtx_bindless_bind_prim_material' 'Hybrid1 binds prim material SSBO'
check "$VK_HYBRID" 'vk_rtx_bind_prim_uv_ssbo' 'Hybrid1 binds PrimUv SSBO'
check "$VK_HYBRID" 'bindlessMeta' 'Hybrid1 UBO bindlessMeta'
check "$VK_HYBRID" 'viewOrigin\[3\]' 'Hybrid1 UBO world prim count for entity offset'
check "$TR_INIT" 'r_rtxBindless' 'r_rtxBindless cvar registration'
check "$TR_INIT" 'r_rtxBindlessCap' 'r_rtxBindlessCap cvar registration'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All Vulkan RTX wiring checks passed."
