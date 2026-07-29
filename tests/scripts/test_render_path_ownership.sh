#!/usr/bin/env bash
# Clustered Hybrid M1: path ownership + shared cluster wiring gates.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
failures=0

check() {
  if ! grep -q "$2" "$1" 2>/dev/null; then
    echo "FAIL: $3"
    failures=$((failures + 1))
  else
    echo "PASS: $3"
  fi
}

PATH_C="$(idtech3_file renderers/vulkan/vk_render_path.c src/renderers/vulkan/vk_render_path.c)"
PATH_H="$(idtech3_file renderers/vulkan/vk_render_path.h src/renderers/vulkan/vk_render_path.h)"
FP_H="$(idtech3_file renderers/vulkan/vk_forward_plus.h src/renderers/vulkan/vk_forward_plus.h)"
FP_C="$(idtech3_file renderers/vulkan/vk_forward_plus.c src/renderers/vulkan/vk_forward_plus.c)"
DGB="$(idtech3_file renderers/vulkan/vk_deferred_gbuffer.c src/renderers/vulkan/vk_deferred_gbuffer.c)"
SHADE="$(idtech3_file renderers/vulkan/tr_shade.c src/renderers/vulkan/tr_shade.c)"
BACKEND="$(idtech3_file renderers/vulkan/tr_backend.c src/renderers/vulkan/tr_backend.c)"
INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"
MODE_H="$(idtech3_file renderers/vulkan/tr_render_mode_vk.h src/renderers/vulkan/tr_render_mode_vk.h)"
MODE_C="$(idtech3_file renderers/vulkan/tr_render_mode_vk.c src/renderers/vulkan/tr_render_mode_vk.c)"
DIAG="$(idtech3_file renderers/vulkan/diagnostics/tr_init_diagnostics.inc src/renderers/vulkan/diagnostics/tr_init_diagnostics.inc)"
COMP="$(idtech3_file renderers/vulkan/shaders/glsl/deferred_lighting_composite.frag src/renderers/vulkan/shaders/glsl/deferred_lighting_composite.frag)"
GEN="$(idtech3_file renderers/vulkan/shaders/glsl/gen_frag.tmpl src/renderers/vulkan/shaders/glsl/gen_frag.tmpl)"
DOC="$ROOT/docs/RENDERER_PATH_OWNERSHIP.md"
GPU="$(idtech3_file renderers/vulkan/vk_gpu_scene.h src/renderers/vulkan/vk_gpu_scene.h)"

check "$PATH_H" 'R_SelectSurfaceRenderPath' 'path selector declared'
check "$PATH_H" 'RENDER_PATH_DEFERRED_OPAQUE' 'path enum includes deferred opaque'
check "$PATH_H" 'r_renderPathDebug' 'r_renderPathDebug cvar'
check "$PATH_H" 'r_hybridCompare' 'r_hybridCompare cvar'
check "$PATH_C" 'R_SelectSurfaceRenderPath' 'path selector implemented'
check "$PATH_C" 'render_path_status' 'render_path_status command'
check "$SHADE" 'R_SelectSurfaceRenderPath' 'shade uses path selector for handoff'
check "$SHADE" 'R_RenderPath_WantsDeferredHandoff' 'handoff from selected path'
check "$BACKEND" 'R_SelectSurfaceRenderPath' 'backend filter uses path selector'
check "$BACKEND" 'R_RenderPath_BeginFrame' 'per-frame path counts reset'
check "$DGB" 'R_SelectSurfaceRenderPath' 'deferred handoff uses path selector'
check "$DGB" 'vk_cluster_assert_shared_consumers' 'deferred lighting asserts shared clusters'
check "$FP_H" 'vk_cluster_assert_shared_consumers' 'cluster assert declared'
check "$FP_H" 'VK_CLUSTER_TILE_SIZE' 'cluster tile size alias'
check "$FP_C" 'cluster_list_generation' 'cluster list generation tracked'
check "$FP_C" 'VK_FP_RECORD_STRIDE' 'Forward+ record size asserted at init'
check "$INIT" 'r_clusterZSlices' 'r_clusterZSlices alias registered'
check "$INIT" 'r_clusterDebug' 'r_clusterDebug alias registered'
check "$INIT" 'R_RenderPath_RegisterCvars' 'path cvars registered at init'
check "$MODE_H" 'renderModeProfile_t' 'render mode profile ABI declared'
check "$MODE_C" 's_renderModeProfiles' 'render mode profile table implemented'
check "$MODE_C" 'tier_a_certified_raster' 'mode profile names certified raster'
check "$MODE_C" 'tier_c_path_traced_reference' 'mode profile names PT reference'
check "$DIAG" 'mode want' 'renderer status prints mode profile contract'
check "$COMP" 'hybridCompare' 'composite supports hybridCompare discard'
check "$GEN" 'pbrDebugMode.y > 1.5' 'gen_frag hybridCompare left/right handoff'
check "$GEN" 'pbrDebugMode.z >= 0.5' 'gen_frag path debug tint'
check "$DOC" 'R_SelectSurfaceRenderPath' 'ownership doc names selector'
check "$DOC" 'Shipping defaults' 'ownership doc lists mode defaults'
check "$DOC" 'r_clusterZSlices' 'ownership doc lists cluster aliases'
check "$GPU" 'prevTransform' 'gpu scene reserves prevTransform'
check "$GPU" 'Clustered Hybrid M1' 'gpu scene schema contract comment'
check "$ROOT/docs/RENDERERS.md" 'Spine shipping default' 'RENDERERS.md mode-2 default fixed'
check "$ROOT/docs/RENDERER_2027.md" 'Spine shipping default' 'RENDERER_2027.md mode-2 default fixed'
check "$ROOT/docs/UNIFIED_CLUSTERED_RENDERER.md" 'RENDERER_PATH_OWNERSHIP.md' 'unified clustered links ownership doc'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All render path ownership checks passed."
