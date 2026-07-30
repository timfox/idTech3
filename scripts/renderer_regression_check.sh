#!/usr/bin/env bash
set -euo pipefail

# Headless renderer regression checks (repo + optional game base).
#
# Always: verify regression docs, key generated shader blobs, and GLSL tree.
# Optional: if GAME_BASE is set, require listed BSPs from OPTIONAL_GAME_ASSETS.txt.
#
# Usage:
#   ./scripts/renderer_regression_check.sh
#   ./scripts/renderer_regression_check.sh -profile core|game|full
#   GAME_BASE=/path/to/game/base ./scripts/renderer_regression_check.sh
#
# Prerequisites: glslangValidator for GLSL validation (same as smoke_test.sh).

PROFILE="full"
while [[ $# -gt 0 ]]; do
  case "$1" in
    -profile|--profile)
      PROFILE="${2:-full}"
      shift 2
      ;;
    *)
      shift
      ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/../CMakeLists.txt" ]; then
  PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  echo "Error: Could not find project root" >&2
  exit 1
fi

cd "$PROJECT_ROOT"

source "$PROJECT_ROOT/tests/scripts/idtech3_test_paths.sh"
idtech3_test_paths_init "$PROJECT_ROOT"

PASS=0
FAIL=0
pass() { PASS=$((PASS + 1)); echo "  ✓ $1"; }
fail() { FAIL=$((FAIL + 1)); echo "  ✗ $1" >&2; }

echo "=== Renderer regression check (headless) profile=${PROFILE} ==="
echo ""

echo "Build profile manifests:"
if [ -f "$PROJECT_ROOT/cmake/renderers/VulkanCoreSources.cmake" ]; then
  pass "VulkanCoreSources.cmake present"
else
  fail "missing cmake/renderers/VulkanCoreSources.cmake"
fi
if [ -f "$PROJECT_ROOT/cmake/renderers/VulkanExtensionSources.cmake" ]; then
  pass "VulkanExtensionSources.cmake present"
else
  fail "missing cmake/renderers/VulkanExtensionSources.cmake"
fi
case "$PROFILE" in
  core)
    grep -q 'USE_EXPERIMENTAL_RENDERERS OFF' "$PROJECT_ROOT/cmake/IdTech3Profile.cmake" && pass "core disables experimental renderers" || fail "profile core manifest"
    ;;
  game)
    grep -q 'USE_EXPERIMENTAL_RENDERERS OFF' "$PROJECT_ROOT/cmake/IdTech3Profile.cmake" && pass "game disables experimental renderers" || fail "profile game manifest"
    ;;
  full|research)
    grep -q 'USE_EXPERIMENTAL_RENDERERS ON' "$PROJECT_ROOT/cmake/IdTech3Profile.cmake" && pass "full enables experimental renderers" || fail "profile full manifest"
    ;;
  *)
    fail "unknown profile: $PROFILE"
    ;;
esac
echo ""

echo "Repo manifest (docs + generated shaders):"
MANIFEST="$PROJECT_ROOT/docs/samples/renderer_regression/REGRESSION_REPO_MANIFEST.txt"
if [ ! -f "$MANIFEST" ]; then
  fail "Missing manifest: $MANIFEST"
else
  while IFS= read -r line || [ -n "$line" ]; do
    line="${line%$'\r'}"
    [[ -z "$line" || "$line" =~ ^# ]] && continue
    p="$PROJECT_ROOT/$line"
    if [ -f "$p" ]; then
      pass "present: $line"
    else
      fail "missing: $line"
    fi
  done < "$MANIFEST"
fi

echo ""
echo "GLSL stage files (glslangValidator -V):"
if command -v glslangValidator &>/dev/null; then
  shader_dir="$PROJECT_ROOT/renderers/vulkan/shaders/glsl"
  shader_errors=0
  shader_count=0
  while IFS= read -r -d '' shader; do
    shader_count=$((shader_count + 1))
    rel="${shader#"$PROJECT_ROOT"/}"
    if ! err="$(glslangValidator -V "$shader" -o /dev/null 2>&1)"; then
      echo "$err" >&2
      fail "GLSL: $rel"
      shader_errors=$((shader_errors + 1))
    fi
  done < <(find "$shader_dir" -type f \( -name '*.vert' -o -name '*.frag' -o -name '*.geom' -o -name '*.comp' \) -print0 | sort -z)
  if [ "$shader_count" -eq 0 ]; then
    fail "No GLSL files under $shader_dir"
  elif [ "$shader_errors" -eq 0 ]; then
    pass "all $shader_count GLSL stages validate"
  fi
else
  echo "  ⚠ glslangValidator not found, skipping GLSL validation" >&2
fi

echo ""
echo "GPU morph SSBO: IQM_MORPH_TOP_K (C vs GLSL must match):"
TR_LOCAL="$PROJECT_ROOT/renderers/vulkan/tr_local.h"
GEN_VERT="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/gen_vert.tmpl"
LIGHT_VERT="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/light_vert.tmpl"
k_c="$(sed -n 's/^#define IQM_MORPH_TOP_K[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$TR_LOCAL" | head -1)"
k_g="$(sed -n 's/^[[:space:]]*const int IQM_MORPH_TOP_K[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*;$/\1/p' "$GEN_VERT" | head -1)"
k_l="$(sed -n 's/^[[:space:]]*const int IQM_MORPH_TOP_K[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*;$/\1/p' "$LIGHT_VERT" | head -1)"
if [[ -z "$k_c" || -z "$k_g" || -z "$k_l" ]]; then
  fail "could not parse IQM_MORPH_TOP_K from tr_local.h / gen_vert.tmpl / light_vert.tmpl"
elif [[ "$k_c" != "$k_g" || "$k_c" != "$k_l" ]]; then
  fail "IQM_MORPH_TOP_K mismatch: tr_local.h=$k_c gen_vert.tmpl=$k_g light_vert.tmpl=$k_l"
else
  pass "IQM_MORPH_TOP_K=$k_c (C + both vertex templates)"
fi

echo ""
echo "glTF morph cap: GLTF_MAX_MORPH_TARGETS vs IQM_MORPH_TOP_K (GPU SSBO packing):"
GLTF_H="$PROJECT_ROOT/renderers/vulkan/tr_model_gltf.h"
gltf_k="$(sed -n 's/^#define GLTF_MAX_MORPH_TARGETS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$GLTF_H" | head -1)"
if [[ -z "$gltf_k" || -z "$k_c" ]]; then
  fail "could not parse GLTF_MAX_MORPH_TARGETS from tr_model_gltf.h or IQM_MORPH_TOP_K from tr_local.h"
elif [[ "$gltf_k" != "$k_c" ]]; then
  fail "GLTF_MAX_MORPH_TARGETS ($gltf_k) != IQM_MORPH_TOP_K ($k_c) - GPU morph SSBO layout will disagree with glTF loader"
else
  pass "GLTF_MAX_MORPH_TARGETS=$gltf_k matches IQM_MORPH_TOP_K"
fi

echo ""
echo "glTF joint cap: GLTF_MAX_JOINTS vs IQM_MAX_JOINTS (skin matrix layout):"
IQM_H="$PROJECT_ROOT/renderers/vulkan/iqm.h"
iqm_j="$(sed -n 's/^#define[[:space:]]*IQM_MAX_JOINTS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$IQM_H" | head -1)"
gltf_j="$(sed -n 's/^#define GLTF_MAX_JOINTS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$GLTF_H" | head -1)"
if [[ -z "$gltf_j" || -z "$iqm_j" ]]; then
  fail "could not parse GLTF_MAX_JOINTS from tr_model_gltf.h or IQM_MAX_JOINTS from iqm.h"
elif [[ "$gltf_j" != "$iqm_j" ]]; then
  fail "GLTF_MAX_JOINTS ($gltf_j) != IQM_MAX_JOINTS ($iqm_j) - skinning matrix buffers disagree between glTF and IQM paths"
else
  pass "GLTF_MAX_JOINTS=$gltf_j matches IQM_MAX_JOINTS"
fi

echo ""
echo "glTF GPU tangent topo: GLTF_GPU_ADJ_TRIS_MAX (C vs gen_vert.tmpl):"
TOPO_H="$PROJECT_ROOT/renderers/vulkan/tr_gltf_topo.h"
GEN_VERT="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/gen_vert.tmpl"
adj_c="$(sed -n 's/^#define GLTF_GPU_ADJ_TRIS_MAX[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$TOPO_H" | head -1)"
adj_glsl="$(grep -E 'const uint GLTF_GPU_ADJ_TRIS_MAX' "$GEN_VERT" | head -1 | sed -n 's/.*GLTF_GPU_ADJ_TRIS_MAX = \([0-9][0-9]*\)u.*/\1/p')"
if [[ -z "$adj_c" || -z "$adj_glsl" ]]; then
  fail "could not parse GLTF_GPU_ADJ_TRIS_MAX from tr_gltf_topo.h or gen_vert.tmpl"
elif [[ "$adj_c" != "$adj_glsl" ]]; then
  fail "GLTF_GPU_ADJ_TRIS_MAX mismatch: C=$adj_c GLSL=$adj_glsl"
else
  pass "GLTF_GPU_ADJ_TRIS_MAX=$adj_c (C + GLSL)"
fi

echo ""
echo "IQM / morph constants (Vulkan renderer):"
if [[ -z "$iqm_j" ]]; then
  fail "could not parse IQM_MAX_JOINTS from vulkan/iqm.h"
else
  pass "IQM_MAX_JOINTS=$iqm_j (vulkan/iqm.h)"
fi
ch_vk="$(sed -n 's/^#define IQM_MORPH_MAX_CHANNELS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$TR_LOCAL" | head -1)"
if [[ -z "$k_c" ]]; then
  fail "could not parse IQM_MORPH_TOP_K from vulkan/tr_local.h"
else
  pass "IQM_MORPH_TOP_K=$k_c (vulkan/tr_local.h)"
fi
if [[ -z "$ch_vk" ]]; then
  fail "could not parse IQM_MORPH_MAX_CHANNELS from vulkan/tr_local.h"
else
  pass "IQM_MORPH_MAX_CHANNELS=$ch_vk (vulkan/tr_local.h)"
fi

echo ""
echo "IQM morph pending slots vs GPU top-K (Vulkan tr_local.h):"
mc_top="$(sed -n 's/^#define IQM_MORPH_MAX_CHANNELS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$TR_LOCAL" | head -1)"
if [[ -z "$mc_top" || -z "$k_c" ]]; then
  fail "could not parse IQM_MORPH_MAX_CHANNELS or IQM_MORPH_TOP_K from vulkan/tr_local.h"
elif [[ "$mc_top" != "$k_c" ]]; then
  fail "IQM_MORPH_MAX_CHANNELS ($mc_top) != IQM_MORPH_TOP_K ($k_c) in vulkan/tr_local.h - pending morph slots must match GPU morph top-K packing"
else
  pass "IQM_MORPH_MAX_CHANNELS=$mc_top matches IQM_MORPH_TOP_K (vulkan tr_local.h)"
fi

echo ""
echo "Forward+ tile cull: MAX_LIGHTS vs VK_FP_MAX_GPU_LIGHTS (GPU light record cap):"
FP_H="$PROJECT_ROOT/renderers/vulkan/vk_forward_plus.h"
FP_COMP="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/forward_plus_tile_cull.comp"
max_gpu="$(sed -n 's/^#define VK_FP_MAX_GPU_LIGHTS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$FP_H" | head -1)"
max_sh="$(sed -n 's/^#define MAX_LIGHTS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$FP_COMP" | head -1)"
if [[ -z "$max_gpu" || -z "$max_sh" ]]; then
  fail "could not parse VK_FP_MAX_GPU_LIGHTS from vk_forward_plus.h or MAX_LIGHTS from forward_plus_tile_cull.comp"
elif [[ "$max_sh" != "$max_gpu" ]]; then
  fail "forward_plus_tile_cull MAX_LIGHTS ($max_sh) != VK_FP_MAX_GPU_LIGHTS ($max_gpu)"
else
  pass "forward_plus_tile_cull MAX_LIGHTS=$max_sh matches VK_FP_MAX_GPU_LIGHTS (surface dlightBits still MAX_DLIGHTS)"
fi

echo ""
echo "Forward+ refdef: tr_world must not clamp tr.refdef.num_dlights (GPU pack uses full count):"
TR_WORLD="$PROJECT_ROOT/renderers/vulkan/tr_world.c"
TR_LOCAL="$PROJECT_ROOT/renderers/vulkan/tr_local.h"
if grep -q 'tr\.refdef\.num_dlights = MAX_DLIGHTS' "$TR_WORLD" 2>/dev/null; then
  fail "tr_world.c clamps tr.refdef.num_dlights to MAX_DLIGHTS (breaks Forward+ lights 33-64)"
elif ! grep -q 'R_SurfaceDlightBitsMask' "$TR_WORLD" 2>/dev/null; then
  fail "tr_world.c missing R_SurfaceDlightBitsMask for classic dlightBits culling"
elif ! grep -q 'R_SurfaceDlightBitsMask' "$TR_LOCAL" 2>/dev/null; then
  fail "tr_local.h missing R_SurfaceDlightBitsMask helper"
else
  pass "tr_world preserves refdef.num_dlights; surface mask via R_SurfaceDlightBitsMask"
fi

echo ""
echo "Forward+ depth cull naming: r_forwardPlusHiZ is probe padding, not vk_hiz pyramid:"
FP_C="$PROJECT_ROOT/renderers/vulkan/vk_forward_plus.c"
FP_GLSL="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/forward_plus_tile_cull.comp"
TR_INIT_VK="$PROJECT_ROOT/renderers/vulkan/tr_init.c"
if grep -q 'hierarchical probes\|hierarchical occlusion' "$FP_C" "$FP_GLSL" "$TR_INIT_VK" 2>/dev/null; then
  fail "Forward+ HiZ still uses pyramid/hierarchical wording without sampling vk_hiz"
elif ! grep -q 'forwardPlusHiZProbePad' "$FP_GLSL" 2>/dev/null; then
  fail "forward_plus_tile_cull.comp missing forwardPlusHiZProbePad marker"
elif ! grep -q 'probe pad, not the vk_hiz pyramid' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c r_forwardPlusHiZ description must distinguish probe pad from vk_hiz pyramid"
else
  pass "r_forwardPlusHiZ clearly documented as same-frame probe padding"
fi

echo ""
echo "TAA shader: YCoCg variance clip / neighborhood stats before history blend:"
TAA_FRAG="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/taa.frag"
if ! grep -q 'neighborhoodYCoCgStats\|RGBToYCoCg' "$TAA_FRAG" 2>/dev/null; then
  fail "taa.frag missing YCoCg variance-clip neighborhood stats"
else
  pass "taa.frag uses YCoCg variance clipping for Temporal Reconstruction"
fi

echo ""
echo "r_renderMode latch (tr_render_mode_vk.c):"
TR_INIT_VK="$PROJECT_ROOT/renderers/vulkan/tr_init.c"
RENDER_MODE_C="$PROJECT_ROOT/renderers/vulkan/tr_render_mode_vk.c"
if [[ ! -f "$RENDER_MODE_C" ]]; then
  fail "missing tr_render_mode_vk.c (R_ApplyRenderModeLatch)"
elif ! grep -q 'void R_ApplyRenderModeLatch' "$RENDER_MODE_C" 2>/dev/null; then
  fail "tr_render_mode_vk.c missing R_ApplyRenderModeLatch"
elif ! grep -q 'R_ApplyRenderModeLatch();' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c should call R_ApplyRenderModeLatch() after R_Register"
elif ! grep -q 'R_ApplyRenderModeLatch();' "$PROJECT_ROOT/renderers/vulkan/vk_forward_plus.c" 2>/dev/null; then
  fail "vk_forward_plus.c should call R_ApplyRenderModeLatch() during Forward+ init"
elif ! grep -q 'r_deferredLighting' "$RENDER_MODE_C" 2>/dev/null; then
  fail "tr_render_mode_vk.c should latch deferred lighting"
elif ! grep -q 'deferred opaque + Forward+ transparent' "$RENDER_MODE_C" 2>/dev/null; then
  fail "tr_render_mode_vk.c mode 1 should document deferred opaque + Forward+ transparent"
elif ! grep -q 'Unified Clustered Renderer' "$RENDER_MODE_C" 2>/dev/null; then
  fail "tr_render_mode_vk.c missing r_renderMode 3 Unified Clustered latch"
elif ! grep -q 'CheckRange( r_renderMode, "0", "5"' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c r_renderMode CheckRange should allow 0-5 (Spine 1.2 modes 4/5)"
else
  pass "R_ApplyRenderModeLatch wired (tr_render_mode_vk.c, R_Init, vk_forward_plus, modes 3/4/5)"
fi

echo ""
echo "Unified Clustered Renderer (r_renderMode 3 frame split):"
TB_C="$PROJECT_ROOT/renderers/vulkan/tr_backend.c"
DGB_UC="$PROJECT_ROOT/renderers/vulkan/vk_deferred_gbuffer.c"
GEN_FRAG_UC="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl"
OIT_ACCUM_UC="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag"
OIT_MBOIT_UC="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/oit_accum_mboit.frag"
FP_LIGHT_EVAL_UC="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/forward_plus_light_eval.glsl"
if ! grep -q 'vk_unified_clustered_active' "$DGB_UC" 2>/dev/null; then
  fail "vk_deferred_gbuffer.c missing vk_unified_clustered_active"
elif ! grep -q 'vk_unified_clustered_active' "$TB_C" 2>/dev/null; then
  fail "tr_backend.c missing Unified Clustered opaque/transparent split"
elif ! test -f "$PROJECT_ROOT/config/vulkan_overlay_unified_clustered.cfg"; then
  fail "missing config/vulkan_overlay_unified_clustered.cfg"
elif ! test -f "$PROJECT_ROOT/config/modern_clustered.cfg"; then
  fail "missing config/modern_clustered.cfg"
elif ! test -f "$PROJECT_ROOT/docs/UNIFIED_CLUSTERED_RENDERER.md"; then
  fail "missing docs/UNIFIED_CLUSTERED_RENDERER.md"
elif ! grep -q 'pbrDebugMode.y' "$GEN_FRAG_UC" 2>/dev/null; then
  fail "gen_frag.tmpl missing mode 3 hybrid handoff (pbrDebugMode.y)"
elif ! grep -q 'forward_plus_cluster.glsl' "$GEN_FRAG_UC" 2>/dev/null; then
  fail "gen_frag.tmpl must include forward_plus_cluster.glsl for Z-slice parity"
elif ! grep -q 'fp_cluster_index' "$GEN_FRAG_UC" 2>/dev/null; then
  fail "gen_frag.tmpl must use fp_cluster_index"
elif ! grep -q 'Soft-cap Forward+ specular' "$GEN_FRAG_UC" 2>/dev/null; then
  fail "gen_frag.tmpl missing Forward+ specular soft-cap (deferred parity)"
elif ! grep -q 'forward_plus_cluster.glsl' "$OIT_ACCUM_UC" 2>/dev/null; then
  fail "oit_accum.frag must include forward_plus_cluster.glsl"
elif ! grep -q 'forward_plus_light_eval.glsl' "$OIT_ACCUM_UC" 2>/dev/null || ! grep -q 'fp_cluster_index' "$FP_LIGHT_EVAL_UC" 2>/dev/null; then
  fail "oit_accum.frag must route Forward+ lighting through shared fp_cluster_index helper"
elif ! grep -q 'forward_plus_cluster.glsl' "$OIT_MBOIT_UC" 2>/dev/null; then
  fail "oit_accum_mboit.frag must include forward_plus_cluster.glsl"
elif ! grep -q 'r_oitForwardPlus' "$TB_C" 2>/dev/null; then
  fail "tr_backend.c OIT path should document r_oitForwardPlus"
else
  pass "Unified Clustered Renderer (mode 3) wired + shared cluster GLSL + soft-cap"
fi

echo ""
echo "2027 visibility-buffer foundation (r_visibilityBuffer):"
VIS_C="$PROJECT_ROOT/renderers/vulkan/vk_visibility_buffer.c"
if ! test -f "$VIS_C"; then
  fail "missing renderers/vulkan/vk_visibility_buffer.c"
elif ! grep -q 'r_visibilityBuffer = ri.Cvar_Get' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_visibilityBuffer cvar"
elif ! grep -q 'vk_visibility_buffer_capture_after_geometry' "$TB_C" 2>/dev/null; then
  fail "tr_backend.c missing visibility buffer capture"
elif ! test -f "$PROJECT_ROOT/config/vulkan_overlay_visibility_2027.cfg"; then
  fail "missing config/vulkan_overlay_visibility_2027.cfg"
elif ! test -f "$PROJECT_ROOT/docs/RENDERER_2027.md"; then
  fail "missing docs/RENDERER_2027.md"
else
  pass "2027 visibility-buffer foundation wired"
fi

echo ""
echo "TAA frame gating (history confidence, motion barrier):"
VK_FRAME_END="$PROJECT_ROOT/renderers/vulkan/vk_frame_end.c"
VK_POSTFX="$PROJECT_ROOT/renderers/vulkan/vk_postfx_params.c"
if ! grep -q 'taaParams\[0\]' "$VK_POSTFX" 2>/dev/null || \
   ! awk '/taaParams\[0\]/,0' "$VK_POSTFX" | grep -q 'unreliableMotionThisFrame'; then
  fail "vk_postfx_params.c must gate TAA history confidence (taaParams[0]) on unreliableMotionThisFrame"
elif grep -q '!vk.temporal.unreliableMotionThisFrame' "$VK_FRAME_END" 2>/dev/null; then
  fail "vk_frame_end.c must not skip the whole TAA pass on unreliableMotionThisFrame (use taaParams confidence)"
elif ! grep -q 'vk_barrier_motion_vector_for_sampling' "$VK_FRAME_END" 2>/dev/null; then
  fail "vk_frame_end.c missing motion-vector barrier before TAA"
elif ! grep -q 'vk_motion_resolve_entity' "$PROJECT_ROOT/renderers/vulkan/vk_view_state.c" 2>/dev/null || \
     ! grep -q 'vk_motion_invalid_reason_name' "$PROJECT_ROOT/renderers/vulkan/vk_view_state.c" 2>/dev/null; then
  fail "vk_view_state.c missing per-entity motion reliability notes"
else
  pass "TAA uses per-frame history confidence; per-entity motion policy wired"
fi

echo ""
echo "Layered AA policy (r_aaMode + SMAA baseline):"
AA_POLICY="$PROJECT_ROOT/renderers/vulkan/vk_aa_policy.c"
if ! test -f "$AA_POLICY"; then
  fail "missing renderers/vulkan/vk_aa_policy.c"
elif ! grep -q 'r_aaMode = ri.Cvar_Get' "$AA_POLICY" 2>/dev/null; then
  fail "vk_aa_policy.c missing r_aaMode cvar"
elif ! grep -q 'vk_aa_policy_apply' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c must call vk_aa_policy_apply"
elif ! grep -q 'exec modern_vulkan_stable.cfg' "$PROJECT_ROOT/config/modern_vulkan.cfg" 2>/dev/null; then
  fail "modern_vulkan.cfg must exec modern_vulkan_stable.cfg (Spine default)"
elif ! grep -q 'seta r_aaMode 2' "$PROJECT_ROOT/config/modern_vulkan_stable.cfg" 2>/dev/null; then
  fail "modern_vulkan_stable.cfg must default r_aaMode 2 (SMAA 1x)"
elif ! grep -q 'seta r_taa 0' "$PROJECT_ROOT/config/modern_vulkan_stable.cfg" 2>/dev/null; then
  fail "modern_vulkan_stable.cfg must default r_taa 0 (SMAA baseline)"
elif ! grep -q 'seta r_renderMode 3' "$PROJECT_ROOT/config/modern_vulkan_stable.cfg" 2>/dev/null; then
  fail "modern_vulkan_stable.cfg must default Unified Clustered mode 3"
elif ! test -f "$PROJECT_ROOT/config/gfx_safe.cfg"; then
  fail "missing config/gfx_safe.cfg recovery profile"
elif ! grep -q 'vk_get_render_target_width' "$PROJECT_ROOT/renderers/vulkan/vk_post_aa.c" 2>/dev/null; then
  fail "vk_post_aa.c must size SMAA from render target extent"
elif ! grep -q 'RF_FIRST_PERSON' "$PROJECT_ROOT/renderers/vulkan/vk_view_state.c" 2>/dev/null || \
     ! grep -q 'VK_MOTION_INVALID_FIRST_PERSON' "$PROJECT_ROOT/renderers/vulkan/vk_view_state.c" 2>/dev/null || \
     ! grep -q 'weaponMatricesHavePrev' "$PROJECT_ROOT/renderers/vulkan/vk_view_state.c" 2>/dev/null; then
  fail "RF_FIRST_PERSON must not poison whole-frame temporal motion"
elif ! grep -q 'RGBToYCoCg\|YCoCg' "$TAA_FRAG" 2>/dev/null; then
  fail "taa.frag missing YCoCg variance clip path"
elif ! test -f "$PROJECT_ROOT/config/vulkan_overlay_temporal_recon.cfg"; then
  fail "missing config/vulkan_overlay_temporal_recon.cfg"
elif ! test -f "$PROJECT_ROOT/config/vulkan_overlay_aa_sharp.cfg"; then
  fail "missing config/vulkan_overlay_aa_sharp.cfg"
elif ! test -f "$PROJECT_ROOT/config/vulkan_overlay_temporal_perf.cfg"; then
  fail "missing config/vulkan_overlay_temporal_perf.cfg"
elif grep -q 'r_hybrid1_taa && r_hybrid1_taa->integer && vk_hybrid1_active' "$VK_FRAME_END" 2>/dev/null; then
  fail "vk_frame_end.c must not auto-force world TAA from r_hybrid1_taa (Hybrid1 owns separate histories)"
else
  pass "Layered AA: r_aaMode policy, SMAA baseline, weapon isolation, Temporal Reconstruction presets"
fi

echo ""
echo "UI overlay compose after tonemap (HUD/menu 2D):"
if ! grep -q 'uiOverlayContentValid' "$VK_FRAME_END" 2>/dev/null; then
  fail "vk_frame_end.c must compose UI overlay when uiOverlayContentValid (not only uiOverlayActive)"
elif ! grep -q 'prepare_post_leave_ui_overlay' "$VK_FRAME_END" 2>/dev/null; then
  fail "vk_frame_end.c must leave UI overlay recording before post process"
elif ! grep -q 'overlay_compose' "$VK_FRAME_END" 2>/dev/null; then
  fail "vk_frame_end.c missing overlay_compose after gamma"
elif ! grep -q 'vk_can_use_2d_overlay_path' "$PROJECT_ROOT/renderers/vulkan/vk_2d_transition.c" 2>/dev/null; then
  fail "vk_2d_transition.c missing menu-safe overlay gating"
else
  pass "HUD/menu UI overlay survives post AA/TAA and composites after gamma"
fi

echo ""
echo "Renderer debug views lifecycle:"
DBG_VIEWS="$PROJECT_ROOT/renderers/vulkan/vk_debug_views.c"
if ! test -f "$DBG_VIEWS"; then
  fail "missing renderers/vulkan/vk_debug_views.c"
elif ! grep -q 'vk_debug_views.c' "$PROJECT_ROOT/cmake/renderers/VulkanCoreSources.cmake" 2>/dev/null; then
  fail "vk_debug_views.c must be documented in Vulkan core source manifest"
elif ! grep -q '#include "vk_debug_views.h"' "$TR_INIT_VK" 2>/dev/null || \
     ! grep -q 'vk_debug_views_init' "$TR_INIT_VK" 2>/dev/null || \
     ! grep -q 'vk_debug_views_shutdown' "$TR_INIT_VK" 2>/dev/null; then
  fail "debug views must be initialized and shut down with renderer lifecycle"
elif ! grep -q '#include "vk_debug_views.h"' "$PROJECT_ROOT/renderers/vulkan/vk_frame_submit.c" 2>/dev/null || \
     ! grep -q 'vk_debug_views_begin_frame' "$PROJECT_ROOT/renderers/vulkan/vk_frame_submit.c" 2>/dev/null; then
  fail "debug views must update once per Vulkan frame"
elif ! grep -q 'ri.Cmd_AddCommand( "debug_view"' "$DBG_VIEWS" 2>/dev/null || \
     ! grep -q 'ri.Cmd_RemoveCommand( "debug_view"' "$DBG_VIEWS" 2>/dev/null; then
  fail "debug_view command must have balanced add/remove lifecycle"
elif grep -q 'TODO: Implement debug view pass recording\|TODO: Return the appropriate image view' "$DBG_VIEWS" 2>/dev/null; then
  fail "debug views must not ship TODO-only pass/image hooks"
elif ! grep -q 'vk.depth_image_view_sample' "$DBG_VIEWS" 2>/dev/null || \
     ! grep -q 'vk.reactive_mask_view' "$DBG_VIEWS" 2>/dev/null || \
     ! grep -q 'vk.taa_history_image_view' "$DBG_VIEWS" 2>/dev/null; then
  fail "debug views must resolve real renderer image views"
else
  pass "debug views lifecycle, command API, and image-view routing wired"
fi

echo ""
echo "Deferred G-buffer fill (r_deferredGBufferFill + compute capture):"
DGB_C="$PROJECT_ROOT/renderers/vulkan/vk_deferred_gbuffer.c"
DGB_COMP="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/deferred_gbuffer_fill.comp"
VK_ATTACH="$PROJECT_ROOT/renderers/vulkan/vk_attachments.c"
if [[ ! -f "$DGB_C" ]]; then
  fail "missing vk_deferred_gbuffer.c"
elif ! grep -q 'r_deferredGBufferFill = ri.Cvar_Get' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_deferredGBufferFill cvar"
elif ! grep -q 'vk_deferred_gbuffer_capture_after_geometry' "$PROJECT_ROOT/renderers/vulkan/tr_backend.c" 2>/dev/null; then
  fail "tr_backend.c should call vk_deferred_gbuffer_capture_after_geometry after geometry"
elif ! grep -q 'deferred_gbuffer_fill_cs' "$PROJECT_ROOT/scripts/compile_shaders.sh" 2>/dev/null; then
  fail "compile_shaders.sh missing deferred_gbuffer_fill_cs"
elif [[ ! -f "$DGB_COMP" ]]; then
  fail "missing deferred_gbuffer_fill.comp"
elif ! grep -q 'vk_deferred_gbuffer_draw_debug' "$DGB_C" 2>/dev/null; then
  fail "vk_deferred_gbuffer.c missing vk_deferred_gbuffer_draw_debug"
elif ! grep -q 'r_deferredGBufferDebug = ri.Cvar_Get' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_deferredGBufferDebug cvar"
elif ! grep -q 'deferred_gbuffer_debug_fs' "$PROJECT_ROOT/scripts/compile_shaders.sh" 2>/dev/null; then
  fail "compile_shaders.sh missing deferred_gbuffer_debug_fs"
else
  pass "deferred G-buffer fill path wired (cvar, backend hook, compute shader)"
fi

echo ""
echo "Deferred lighting (r_deferredLighting + Forward+ tile diffuse):"
if ! grep -q 'r_deferredLighting = ri.Cvar_Get' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_deferredLighting cvar"
elif ! grep -q 'vk_deferred_lighting_apply_after_geometry' "$PROJECT_ROOT/renderers/vulkan/tr_backend.c" 2>/dev/null; then
  fail "tr_backend.c should call vk_deferred_lighting_apply_after_geometry after G-buffer capture"
elif ! grep -q 'deferred_lighting_cs' "$PROJECT_ROOT/scripts/compile_shaders.sh" 2>/dev/null; then
  fail "compile_shaders.sh missing deferred_lighting_cs"
elif [[ ! -f "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/deferred_lighting.comp" ]]; then
  fail "missing deferred_lighting.comp"
elif ! grep -q 'deferred_lighting_image' "$VK_ATTACH" 2>/dev/null; then
  fail "vk_attachments.c missing deferred_lighting_image alloc"
elif ! grep -q 'r_deferredUnlitBase = ri.Cvar_Get' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_deferredUnlitBase cvar"
elif ! grep -q 'vk_deferred_unlit_base_wanted' "$DGB_C" 2>/dev/null; then
  fail "vk_deferred_gbuffer.c missing vk_deferred_unlit_base_wanted"
elif ! grep -q 'pc.additive\|additive' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" 2>/dev/null; then
  fail "deferred_lighting_common.glsl missing additive composite path"
elif ! grep -q 'matClass == CLASS_TRANSMISSION' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" 2>/dev/null || \
     ! grep -q 'Transmission/refraction stay Forward+ owned' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" 2>/dev/null; then
  fail "deferred lighting must skip transmission-class pixels and leave them Forward+ owned"
elif ! grep -q 'lightmapMode' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/deferred_lighting.comp" 2>/dev/null || \
     ! grep -q 'lightmapMode' "$DGB_C" 2>/dev/null || \
     ! grep -q 'lightmapMode' "$PROJECT_ROOT/renderers/vulkan/vk_vrcs.c" 2>/dev/null || \
     ! grep -q 'DeferredStaticDiffuseFromDeluxeApprox' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/lightmap_decode.glsl" 2>/dev/null; then
  fail "deferred deluxe lightmap mode must be threaded through standard + VRCS compute paths"
elif ! grep -q 'r_deferredLightingStrength = ri.Cvar_Get' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_deferredLightingStrength cvar"
elif ! grep -q 'vk_deferred_unlit_base_wanted' "$PROJECT_ROOT/renderers/vulkan/tr_shade.c" 2>/dev/null; then
  fail "tr_shade.c should skip ProjectDlightTexture when deferred unlit base active"
elif ! grep -q 'deferred_unlit_base_strength' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" 2>/dev/null; then
  fail "gen_frag.tmpl missing deferred_unlit_base_strength fragment spec constant"
elif ! grep -q 'deferred_unlit_base_strength' "$PROJECT_ROOT/renderers/vulkan/vk_create_pipeline.c" 2>/dev/null; then
  fail "vk_create_pipeline.c missing deferred_unlit_base_strength specialization"
elif ! grep -q 'r_deferredSpecular = ri.Cvar_Get' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_deferredSpecular cvar"
elif ! grep -q 'sceneBaseTex' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_composite.frag" 2>/dev/null; then
  fail "deferred_lighting_composite.frag missing sceneBaseTex additive blend"
elif ! grep -q 'pc.specular\|specularStrength' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" 2>/dev/null; then
  fail "deferred_lighting_common.glsl missing specular toggle"
elif ! grep -q 'Diffuse_Burley' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" 2>/dev/null; then
  fail "deferred_lighting_common.glsl missing Diffuse_Burley (Forward+ Fd parity)"
elif ! grep -q 'kD' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" 2>/dev/null; then
  fail "deferred_lighting_common.glsl missing Fresnel kD energy term"
else
  pass "deferred lighting compute + composite + Burley Fd parity wired"
fi

echo ""
echo "Deferred many-lights demo:"
if ! bash "$PROJECT_ROOT/tests/scripts/test_deferred_many_lights_demo.sh" >/tmp/deferred_many_lights_demo_check.log 2>&1; then
  cat /tmp/deferred_many_lights_demo_check.log
  fail "deferred many-lights demo static gate failed"
else
  pass "deferred many-lights demo config + renderer-owned 64-light injector wired"
fi

echo ""
echo "G-buffer / Ambient Visibility lifecycle:"
if [[ ! -x "$PROJECT_ROOT/scripts/gbuffer_av_lifecycle_check.sh" && ! -f "$PROJECT_ROOT/scripts/gbuffer_av_lifecycle_check.sh" ]]; then
  fail "missing scripts/gbuffer_av_lifecycle_check.sh"
elif ! bash "$PROJECT_ROOT/scripts/gbuffer_av_lifecycle_check.sh"; then
  fail "gbuffer_av_lifecycle_check.sh failed"
else
  pass "G-buffer/AV lifecycle static contract"
fi

echo ""
echo "Temporal history ownership:"
if [[ ! -f "$PROJECT_ROOT/scripts/temporal_ownership_check.sh" ]]; then
  fail "missing scripts/temporal_ownership_check.sh"
elif ! bash "$PROJECT_ROOT/scripts/temporal_ownership_check.sh"; then
  fail "temporal_ownership_check.sh failed"
else
  pass "temporal history ownership static contract"
fi

echo ""
echo "Pass/resource registry:"
if [[ ! -f "$PROJECT_ROOT/scripts/pass_registry_check.sh" ]]; then
  fail "missing scripts/pass_registry_check.sh"
elif ! bash "$PROJECT_ROOT/scripts/pass_registry_check.sh"; then
  fail "pass_registry_check.sh failed"
else
  pass "pass/resource registry static contract"
fi

echo ""
echo "Spine combination matrix:"
if [[ ! -f "$PROJECT_ROOT/scripts/spine_combo_matrix_check.sh" ]]; then
  fail "missing scripts/spine_combo_matrix_check.sh"
elif ! bash "$PROJECT_ROOT/scripts/spine_combo_matrix_check.sh"; then
  fail "spine_combo_matrix_check.sh failed"
else
  pass "spine combination matrix static contract"
fi

echo ""
echo "Spine 1.1 certification:"
if [[ ! -f "$PROJECT_ROOT/scripts/spine_1_1_cert_check.sh" ]]; then
  fail "missing scripts/spine_1_1_cert_check.sh"
elif ! bash "$PROJECT_ROOT/scripts/spine_1_1_cert_check.sh"; then
  fail "spine_1_1_cert_check.sh failed"
else
  pass "Spine 1.1 certification static contract"
fi

echo ""
echo "Spine 1.2 mode model:"
if [[ ! -f "$PROJECT_ROOT/scripts/spine_1_2_mode_check.sh" ]]; then
  fail "missing scripts/spine_1_2_mode_check.sh"
elif ! bash "$PROJECT_ROOT/scripts/spine_1_2_mode_check.sh"; then
  fail "spine_1_2_mode_check.sh failed"
else
  pass "Spine 1.2 mode model static contract"
fi

echo ""
echo "Spine platform restore hooks:"
if [[ ! -f "$PROJECT_ROOT/scripts/spine_platform_restore_check.sh" ]]; then
  fail "missing scripts/spine_platform_restore_check.sh"
elif ! bash "$PROJECT_ROOT/scripts/spine_platform_restore_check.sh"; then
  fail "spine_platform_restore_check.sh failed"
else
  pass "spine platform restore-hook static contract"
fi

echo ""
echo "Directional Ambient Visibility (GTAO / RTAO / Reference AO):"
AV_C="$PROJECT_ROOT/renderers/vulkan/vk_ambient_visibility.c"
AV_GLSL="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/ambient_visibility"
if [[ ! -f "$AV_C" || ! -f "$PROJECT_ROOT/renderers/vulkan/vk_ambient_visibility.h" ]]; then
  fail "missing Ambient Visibility host module"
elif ! grep -q 'r_ambientVisibilityMode' "$AV_C" 2>/dev/null || \
     ! grep -q 'r_referenceAORays' "$AV_C" 2>/dev/null; then
  fail "Ambient Visibility mode/reference cvars are missing"
elif ! grep -q 'vk_ambient_visibility_apply_after_geometry' "$PROJECT_ROOT/renderers/vulkan/tr_backend.c" 2>/dev/null; then
  fail "tr_backend.c missing Ambient Visibility opaque-geometry hook"
elif ! grep -q 'vk_ambient_visibility_blocks_legacy_post' "$PROJECT_ROOT/renderers/vulkan/vk_postfx_passes.c" 2>/dev/null; then
  fail "legacy SSAO is not mutually exclusive with Ambient Visibility"
elif [[ ! -f "$AV_GLSL/av_gtao.comp" || ! -f "$AV_GLSL/av_rtao.comp" || \
        ! -f "$AV_GLSL/av_temporal.comp" || ! -f "$AV_GLSL/av_filter.comp" || \
        ! -f "$AV_GLSL/av_composite.comp" ]]; then
  fail "Ambient Visibility shader suite is incomplete"
elif ! grep -q 'rayQueryInitializeEXT' "$AV_GLSL/av_rtao.comp" 2>/dev/null; then
  fail "RTAO shader missing ray-query traversal"
elif ! grep -q 'historyGeo' "$AV_GLSL/av_temporal.comp" 2>/dev/null || \
     ! grep -q 'motionTex' "$AV_GLSL/av_temporal.comp" 2>/dev/null; then
  fail "Ambient Visibility temporal pass missing dedicated geometry/motion history"
elif ! grep -Eq 'CLASS_TRANSMISSION[[:space:]]*=[[:space:]]*3u' "$AV_GLSL/av_composite.comp" 2>/dev/null || \
     ! grep -Eq 'CLASS_EMISSIVE[[:space:]]*=[[:space:]]*4u' "$AV_GLSL/av_composite.comp" 2>/dev/null; then
  fail "Ambient Visibility composite missing transmission/emissive exclusion policy"
elif ! grep -q 'vk_ambient_visibility_view' "$PROJECT_ROOT/renderers/vulkan/extensions/rtx/vk_rcgi.c" 2>/dev/null || \
     ! grep -q 'vk_ambient_visibility_view' "$PROJECT_ROOT/renderers/vulkan/extensions/rtx/vk_surfel_gi.c" 2>/dev/null; then
  fail "RcGI/Surfel GI do not consume shared Ambient Visibility"
elif ! grep -q 'vk_av_rtao_cs_spv' "$PROJECT_ROOT/renderers/vulkan/vk_ambient_visibility_spirv.inc" 2>/dev/null; then
  fail "Ambient Visibility embedded SPIR-V is missing"
else
  pass "directional AV modes, dedicated history, GI consumers, fallback, debug/reference shaders wired"
fi

echo ""
echo "Deferred G-buffer scaffold (r_renderMode 1 + r_deferredGBuffer):"
if ! grep -q 'r_deferredGBuffer = ri.Cvar_Get' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_deferredGBuffer cvar"
elif ! grep -q 'vk_create_deferred_gbuffer_scaffold' "$VK_ATTACH" 2>/dev/null; then
  fail "vk_attachments.c missing vk_create_deferred_gbuffer_scaffold"
elif ! grep -q 'deferred_gbuffer_albedo' "$VK_ATTACH" 2>/dev/null; then
  fail "vk_attachments.c missing deferred_gbuffer_albedo destroy path"
else
  pass "r_deferredGBuffer cvar + deferred G-buffer scaffold alloc/teardown"
fi

echo ""
echo "Conditional #else stubs (RTX / FreeType / experimental / Steam):"
CMAKE_ROOT="$PROJECT_ROOT/CMakeLists.txt"
if ! grep -q 'USE_EXPERIMENTAL_RENDERERS' "$CMAKE_ROOT" 2>/dev/null; then
  fail "CMakeLists.txt missing USE_EXPERIMENTAL_RENDERERS"
elif ! grep -q 'vk_experimental_renderer_stubs.c' "$CMAKE_ROOT" 2>/dev/null; then
  fail "CMakeLists.txt missing vk_experimental_renderer_stubs.c wiring"
elif ! grep -q 'tr_vector_font_stub.c' "$CMAKE_ROOT" 2>/dev/null; then
  fail "CMakeLists.txt missing tr_vector_font_stub.c wiring"
elif ! grep -q 'USE_VULKAN_RTX=ON' "$PROJECT_ROOT/renderers/vulkan/extensions/rtx/vk_rtx.c" 2>/dev/null; then
  fail "vk_rtx.c missing RTX-off stub log"
elif ! grep -q 'BUILD_FREETYPE=ON' "$PROJECT_ROOT/renderers/common/tr_font_stub.c" 2>/dev/null; then
  fail "tr_font_stub.c missing FreeType-off stub log"
elif ! grep -q '#else /\* !USE_STEAM \*/' "$IDTECH3_CLIENT/platform/cl_steam.c" 2>/dev/null; then
  fail "cl_steam.c missing USE_STEAM off stub branch"
else
  pass "conditional stub paths wired (RTX, FreeType, experimental, Steam)"
fi

echo ""
echo "Platform renderer roadmap scaffolds (Metal / DXR / WebGPU):"
if ! grep -q 'tr_platform_renderer_stub.c' "$CMAKE_ROOT" 2>/dev/null; then
  fail "CMakeLists.txt missing tr_platform_renderer_stub.c wiring"
elif ! grep -q 'renderer_backend.h' "$IDTECH3_CLIENT/core/cl_ref.c" 2>/dev/null; then
  fail "cl_ref.c missing renderer_backend.h include"
elif ! grep -q 'WEBGPU_ROADMAP.md' "$IDTECH3_CLIENT/core/cl_ref.c" 2>/dev/null; then
  fail "cl_ref.c missing WebGPU roadmap fallback message"
elif ! "$PROJECT_ROOT/scripts/check_webgpu_shader_portability.sh" >/dev/null 2>&1; then
  fail "check_webgpu_shader_portability.sh failed"
else
  pass "Metal/DXR scaffold + WebGPU shader manifest wired"
fi

echo ""
echo "Client modularization (cl_main.c split):"
if ! "$PROJECT_ROOT/tests/scripts/test_client_modular.sh" >/dev/null 2>&1; then
  fail "test_client_modular.sh failed"
else
  pass "cl_main.c slim + lifecycle/frame/cvars modules wired"
fi

echo ""
echo "Engine-native sprite props (misc_billboard / misc_flipbook / misc_imposter / misc_voxel):"
SP_C="$PROJECT_ROOT/renderers/vulkan/tr_sprite_props.c"
TR_TYPES="$PROJECT_ROOT/renderers/common/tr_types.h"
VOX_C="$PROJECT_ROOT/renderers/vulkan/tr_model_vox.c"
if ! grep -q 'misc_billboard' "$SP_C" 2>/dev/null; then
  fail "tr_sprite_props.c missing misc_billboard parse"
elif ! grep -q 'misc_flipbook' "$SP_C" 2>/dev/null; then
  fail "tr_sprite_props.c missing misc_flipbook parse"
elif ! grep -q 'misc_imposter' "$SP_C" 2>/dev/null; then
  fail "tr_sprite_props.c missing misc_imposter parse"
elif ! grep -q 'ENGINE_SPRITE_VOXEL' "$SP_C" 2>/dev/null; then
  fail "tr_sprite_props.c missing ENGINE_SPRITE_VOXEL / misc_voxel"
elif ! grep -q 'misc_voxel' "$PROJECT_ROOT/engine/core/engine_sprite_map.c" 2>/dev/null; then
  fail "engine_sprite_map.c missing misc_voxel classname"
elif ! test -f "$VOX_C" || ! grep -q 'R_RegisterVOX' "$VOX_C" 2>/dev/null; then
  fail "tr_model_vox.c missing R_RegisterVOX"
elif ! grep -q 'voxel_spawn' "$IDTECH3_CLIENT/shell/cl_engine_sprites.c" 2>/dev/null; then
  fail "cl_engine_sprites.c missing voxel_spawn"
elif ! grep -q 'R_SpriteProps_ParseFromEntityString' "$PROJECT_ROOT/renderers/vulkan/tr_bsp.c" 2>/dev/null; then
  fail "tr_bsp.c should parse sprite props on RE_LoadWorldMap"
elif ! grep -q 'RF_SPRITE_YAWLOCK' "$TR_TYPES" 2>/dev/null; then
  fail "tr_types.h missing RF_SPRITE_YAWLOCK"
elif ! grep -q 'RF_SPRITE_FLIPBOOK' "$TR_TYPES" 2>/dev/null; then
  fail "tr_types.h missing RF_SPRITE_FLIPBOOK"
elif ! grep -q 'EF_BILLBOARD' "${PROJECT_ROOT}/engine/core/q_shared.h" 2>/dev/null; then
  fail "q_shared.h missing EF_BILLBOARD engine flag"
elif ! grep -q 'AddEngineSpriteToScene' "$PROJECT_ROOT/renderers/common/tr_public.h" 2>/dev/null; then
  fail "tr_public.h missing AddEngineSpriteToScene export"
elif ! grep -q 'R_SpriteProps_Init' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c should call R_SpriteProps_Init"
elif ! grep -q 'r_spriteProps = ri.Cvar_Get' "$SP_C" 2>/dev/null; then
  fail "tr_sprite_props.c missing r_spriteProps cvar"
elif ! grep -q 'CL_EngineSprites_AddFromSnapshot' "$IDTECH3_CLIENT/core/cl_cgame.c" 2>/dev/null; then
  fail "cl_cgame.c should call CL_EngineSprites_AddFromSnapshot before RenderScene"
elif ! grep -q 'cl_engineSprites = Cvar_Get' "$IDTECH3_CLIENT/shell/cl_engine_sprites.c" 2>/dev/null; then
  fail "cl_engine_sprites.c missing cl_engineSprites cvar"
elif ! grep -q 'CS_ENGINE_SPRITE_SHADERS' "$PROJECT_ROOT/runtime/game/bg_public.h" 2>/dev/null; then
  fail "bg_public.h missing CS_ENGINE_SPRITE_SHADERS"
elif ! grep -q 'EngineSpriteMap_Parse' "${PROJECT_ROOT}/engine/core/engine_sprite_map.c" 2>/dev/null; then
  fail "engine_sprite_map.c missing shared map parser"
elif ! grep -q 'SV_EngineSprites_LoadMap' "$PROJECT_ROOT/runtime/server/core/sv_init.c" 2>/dev/null; then
  fail "sv_init.c should load map sprite shaders on CM_LoadMap"
elif ! grep -q 'SV_EngineSprites_SpawnMapEntities' "$PROJECT_ROOT/runtime/server/core/sv_init.c" 2>/dev/null; then
  fail "sv_init.c should spawn map sprite snapshot ents after game init"
elif ! grep -q 'CS_ENGINE_SPRITE_META' "$PROJECT_ROOT/runtime/game/bg_public.h" 2>/dev/null; then
  fail "bg_public.h missing CS_ENGINE_SPRITE_META"
elif ! grep -q 'G_ENGINE_SPRITE_SPAWN' "$PROJECT_ROOT/runtime/game/g_public.h" 2>/dev/null; then
  fail "g_public.h missing G_ENGINE_SPRITE_SPAWN game trap"
elif ! grep -q 'SV_EngineSprite_SpawnFromDef' "$PROJECT_ROOT/runtime/server/gameplay/sv_engine_sprites.c" 2>/dev/null; then
  fail "sv_engine_sprites.c missing runtime spawn helper"
elif ! grep -q 'registerTable(L, "Sprites"' \
	"$PROJECT_ROOT/runtime/game/scripting/g_lua_bindings.c" \
	"$PROJECT_ROOT/runtime/game/scripting/g_lua_registration.inc" 2>/dev/null; then
  fail "g_lua_bindings.c missing Engine.Sprites Lua table"
elif ! grep -q 'CG_ENGINE_SPRITE_ADD_LOCAL' "$PROJECT_ROOT/runtime/cgame/cg_public.h" 2>/dev/null; then
  fail "cg_public.h missing CG_ENGINE_SPRITE_ADD_LOCAL cgame trap"
elif ! grep -q 'CL_EngineSprite_AddLocalAtTime' "$IDTECH3_CLIENT/shell/cl_engine_sprites.c" 2>/dev/null; then
  fail "cl_engine_sprites.c missing AddLocalAtTime helper for cgame trap"
else
  pass "engine-native sprite props (map entities + RE_AddEngineSpriteToScene)"
fi

echo ""
echo "Engine-native decals (misc_decal):"
DC_C="$PROJECT_ROOT/renderers/vulkan/tr_decal_props.c"
if ! grep -q 'misc_decal' "${PROJECT_ROOT}/engine/core/engine_decal_map.c" 2>/dev/null; then
  fail "engine_decal_map.c missing misc_decal parse"
elif ! grep -q 'EF_DECAL' "${PROJECT_ROOT}/engine/core/q_shared.h" 2>/dev/null; then
  fail "q_shared.h missing EF_DECAL"
elif ! grep -q 'CS_ENGINE_DECAL_SHADERS' "$PROJECT_ROOT/runtime/game/bg_public.h" 2>/dev/null; then
  fail "bg_public.h missing CS_ENGINE_DECAL_SHADERS"
elif ! grep -q 'AddEngineDecalToScene' "$PROJECT_ROOT/renderers/common/tr_public.h" 2>/dev/null; then
  fail "tr_public.h missing AddEngineDecalToScene"
elif ! grep -q 'registerTable(L, "Decals"' \
	"$PROJECT_ROOT/runtime/game/scripting/g_lua_bindings.c" \
	"$PROJECT_ROOT/runtime/game/scripting/g_lua_registration.inc" 2>/dev/null; then
  fail "g_lua_bindings.c missing Engine.Decals Lua table"
else
  pass "engine-native decals (CS + EF_DECAL + renderer bridge)"
fi

echo ""
echo "Network eFlags wire width (engine flags bits 20-23):"
if ! grep -q '{ NETF(eFlags), 24 }' "${PROJECT_ROOT}/engine/core/msg.c" 2>/dev/null; then
  fail "msg.c eFlags must be 24 bits for EF_BILLBOARD..EF_DECAL"
else
  pass "msg.c NETF(eFlags) 24-bit (protocol 72+)"
fi

echo ""
echo "Billboard sprites (EF_BILLBOARD + RT_SPRITE renderer path):"
Q_SHARED="${PROJECT_ROOT}/engine/core/q_shared.h"
TR_TYPES="$PROJECT_ROOT/renderers/common/tr_types.h"
TR_SURF="$PROJECT_ROOT/renderers/vulkan/tr_surface.c"
if ! grep -q 'EF_BILLBOARD' "$Q_SHARED" 2>/dev/null; then
  fail "q_shared.h missing EF_BILLBOARD"
elif ! grep -q 'RT_SPRITE' "$TR_TYPES" 2>/dev/null; then
  fail "tr_types.h missing RT_SPRITE"
elif ! grep -q 'RB_SurfaceSprite' "$TR_SURF" 2>/dev/null; then
  fail "tr_surface.c missing RB_SurfaceSprite"
elif ! grep -q 'RF_SPRITE_FLIPBOOK' "$TR_SURF" 2>/dev/null; then
  fail "tr_surface.c missing flipbook UV path in RB_SurfaceSprite"
elif ! grep -q 'case RT_SPRITE:' "$PROJECT_ROOT/renderers/vulkan/tr_main.c" 2>/dev/null; then
  fail "tr_main.c missing RT_SPRITE draw surf path"
else
  pass "EF_BILLBOARD + RT_SPRITE backend (engine map props + RB_SurfaceSprite)"
fi

echo ""
echo "TAA: motion-vector path (set 4) and depthParams.z gate:"
if ! grep -q 'layout(set = 4, binding = 0) uniform sampler2D motionTex' "$TAA_FRAG" 2>/dev/null; then
  fail "taa.frag missing motionTex binding (set 4)"
elif ! grep -q 'postfx.depthParams.z' "$TAA_FRAG" 2>/dev/null; then
  fail "taa.frag missing postfx.depthParams.z motion-vector gate"
elif ! grep -q 'pipeline_layout_taa' "$PROJECT_ROOT/renderers/vulkan/vk_init_device.c" 2>/dev/null; then
  fail "vk_init_device.c missing pipeline_layout_taa"
else
  pass "TAA motion-vector shader + pipeline_layout_taa present"
fi

echo ""
echo "Temporal reactive mask (R8 stamp + TAA set 5):"
VK_H="$PROJECT_ROOT/renderers/vulkan/vk.h"
VK_REACTIVE="$PROJECT_ROOT/renderers/vulkan/vk_reactive_mask.c"
if ! grep -q 'reactive_mask_image' "$VK_H" 2>/dev/null; then
  fail "vk.h missing reactive_mask_image"
elif ! grep -q 'taa_reactive_descriptor' "$VK_H" 2>/dev/null; then
  fail "vk.h missing taa_reactive_descriptor"
elif ! grep -q 'layout(set = 5, binding = 0) uniform sampler2D reactiveMaskTex' "$TAA_FRAG" 2>/dev/null; then
  fail "taa.frag missing reactiveMaskTex binding (set 5)"
elif ! grep -q 'midsGamma.a' "$TAA_FRAG" 2>/dev/null; then
  fail "taa.frag missing midsGamma.a maskBound gate"
elif ! grep -q 'vk_reactive_mask_clear' "$VK_REACTIVE" 2>/dev/null; then
  fail "vk_reactive_mask.c missing clear helper"
elif ! grep -q 'vk_reactive_mask_stamp_from_reveal' "$VK_REACTIVE" 2>/dev/null; then
  fail "vk_reactive_mask.c missing OIT reveal stamp"
elif ! grep -q 'vk_reactive_mask_clear' "$PROJECT_ROOT/renderers/vulkan/tr_backend.c" 2>/dev/null; then
  fail "tr_backend.c missing reactive mask clear hook"
elif ! grep -q 'vk_reactive_mask_stamp_from_reveal' "$PROJECT_ROOT/renderers/vulkan/vk_postfx_passes.c" 2>/dev/null; then
  fail "vk_postfx_passes.c missing OIT reactive stamp hook"
elif ! grep -q 'set_layouts\[5\] = vk.set_layout_sampler' "$PROJECT_ROOT/renderers/vulkan/vk_init_device.c" 2>/dev/null; then
  fail "vk_init_device.c TAA layout missing set 5 sampler"
elif ! grep -q 'reactiveMaskImg' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" 2>/dev/null; then
  fail "gen_frag.tmpl missing reactiveMaskImg storage image"
else
  pass "Temporal reactive mask: R8 attach, clear/stamp, TAA set 5, gen_frag store"
fi

echo ""
echo "Weapon-after-TAA temporal ownership:"
VK_TEMP_C="$PROJECT_ROOT/renderers/vulkan/vk_temporal.c"
VK_BACKEND="$PROJECT_ROOT/renderers/vulkan/tr_backend.c"
VK_FRAME_END="$PROJECT_ROOT/renderers/vulkan/vk_frame_end.c"
if ! grep -q 'vk_temporal_want_weapon_after_taa' "$VK_TEMP_C" 2>/dev/null; then
  fail "vk_temporal.c missing vk_temporal_want_weapon_after_taa"
elif ! grep -q 'RB_TryDeferWeaponDrawSurfs' "$VK_BACKEND" 2>/dev/null; then
  fail "tr_backend.c missing RB_TryDeferWeaponDrawSurfs"
elif ! grep -q 'RB_FlushDeferredWeaponAfterTaa' "$VK_BACKEND" 2>/dev/null; then
  fail "tr_backend.c missing RB_FlushDeferredWeaponAfterTaa"
elif ! grep -q 'RB_FlushDeferredWeaponAfterTaa' "$VK_FRAME_END" 2>/dev/null; then
  fail "vk_frame_end.c missing weapon flush after TAA"
elif ! grep -q 'r_temporalWeaponAfterTaa' "$PROJECT_ROOT/renderers/vulkan/vk_aa_policy.c" 2>/dev/null; then
  fail "vk_aa_policy.c missing r_temporalWeaponAfterTaa registration"
elif ! grep -q 'stochMode >= 2 && !vk_temporal_reconstruction_wanted' "$PROJECT_ROOT/renderers/vulkan/vk_view_state.c" 2>/dev/null; then
  fail "vk_view_state.c missing stochastic mode-2 fallback when TAA off"
elif ! grep -q 'reactiveHard = adaptive ? 0.65 : 0.82' "$TAA_FRAG" 2>/dev/null || \
     ! grep -q 'reactive > reactiveHard' "$TAA_FRAG" 2>/dev/null; then
  fail "taa.frag missing hard reactive history reject"
else
	pass "Weapon-after-TAA + stochastic TAA-off fallback + reactive hard reject"
fi

echo ""
echo "OIT striping guards (barrier + NEAREST + texelFetch + depth dispatch):"
OIT_RESOLVE="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/oit_resolve.frag"
OIT_PASS="$PROJECT_ROOT/renderers/vulkan/vk_postfx_passes.c"
VK_VOL_INT="$PROJECT_ROOT/renderers/vulkan/vk_volumetric_internal.c"
VK_TEMP="$PROJECT_ROOT/renderers/vulkan/vk_temporal.c"
VK_DESC="$PROJECT_ROOT/renderers/vulkan/vk_descriptor_sets.c"
OIT_WEIGHT="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/oit_weight.glsl"
if ! grep -q 'texelFetch( oitAccumTex' "$OIT_RESOLVE" 2>/dev/null; then
  fail "oit_resolve.frag must texelFetch OIT buffers (not LINEAR textureLod)"
elif ! grep -q 'vk_oit_barrier_targets_for_sampling' "$OIT_PASS" 2>/dev/null; then
  fail "vk_postfx_passes.c missing OIT full-framebuffer sample barrier"
elif ! grep -q 'GL_NEAREST' "$VK_DESC" 2>/dev/null || ! grep -A8 'r_oit && r_oit->integer' "$VK_DESC" 2>/dev/null | grep -q 'GL_NEAREST'; then
  fail "OIT resolve descriptors must use GL_NEAREST"
elif ! grep -q 'vk_get_active_render_extent' "$VK_VOL_INT" 2>/dev/null; then
  fail "MSAA depth resolve must dispatch using active render extent"
elif ! grep -q 'Commit previous-frame matrices once at frame end' "$VK_TEMP" 2>/dev/null; then
  fail "vk_temporal_commit_frame_state must own prev matrix commit"
elif ! grep -q 'vk_temporal_capture_world_viewparms' "$VK_TEMP" 2>/dev/null; then
  fail "world viewparms must be snapshotted before weapon flush for prev-matrix commit"
elif ! grep -Eq 'clamp\([[:space:]]*(base\.a|samp\.opacity)[[:space:]]*,[[:space:]]*0\.0[[:space:]]*,[[:space:]]*0\.999[[:space:]]*\)' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag" 2>/dev/null; then
  fail "WBOIT accum must clamp normalized opacity like MBOIT"
elif ! grep -q 'Depth_ViewDepthToTraditional01' "$OIT_WEIGHT" 2>/dev/null; then
  fail "WBOIT must adapt McGuire weight for reversed-Z (zTrad)"
elif ! grep -q 'clamp( aFactor \* OIT_W_LUMA_SCALE \* zFactor, OIT_W_MIN, OIT_W_MAX )' "$OIT_WEIGHT" 2>/dev/null || \
     ! grep -q 'OIT_W_MIN = 1e-2' "$OIT_WEIGHT" 2>/dev/null || \
     ! grep -q 'OIT_W_MAX = 3e3' "$OIT_WEIGHT" 2>/dev/null; then
  fail "WBOIT weight must clamp [1e-2, 3e3] to prevent fp16 underflow stipple"
elif ! grep -q 'coverage < 1e-4' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/oit_resolve.frag" 2>/dev/null; then
  fail "OIT resolve must skip tiny coverage (underflow stipple guard)"
elif ! grep -q 'Stamp once after final bucket resolve' "$PROJECT_ROOT/renderers/vulkan/vk_postfx_passes.c" 2>/dev/null || \
     ! awk '/vk_begin_render_pass_tracked\( vk.render_pass.oit_resolve/,/vk_reactive_mask_stamp_from_reveal/' "$PROJECT_ROOT/renderers/vulkan/vk_postfx_passes.c" | grep -q 'vk_reactive_mask_stamp_from_reveal'; then
  fail "reactive stamp must run after OIT resolve (not between accum and composite)"
elif grep -q 'Com_Memcpy( vk_prev_viewproj_matrix, params.viewProj' "$PROJECT_ROOT/renderers/vulkan/vk_volumetric_params.c" 2>/dev/null; then
  fail "volumetric must not overwrite shared prev matrices mid-frame"
else
  pass "OIT striping guards: barrier, NEAREST, texelFetch, depth dispatch, matrix commit"
fi

echo ""
echo "OIT resolve framebuffer attachmentCount + generations:"
OIT_FB="$PROJECT_ROOT/renderers/vulkan/vk_framebuffers.c"
OIT_DESC="$PROJECT_ROOT/renderers/vulkan/vk_descriptor_sets.c"
OIT_PASS="$PROJECT_ROOT/renderers/vulkan/vk_postfx_passes.c"
if ! grep -A8 'oit_resolve' "$OIT_FB" 2>/dev/null | grep -q 'attachmentCount = 1'; then
  fail "oit_resolve framebuffer must set attachmentCount=1 (accum leaves 2/3)"
elif ! grep -q 'oitAttachmentGeneration' "$PROJECT_ROOT/renderers/vulkan/vk.h" 2>/dev/null; then
  fail "vk.h missing oitAttachmentGeneration"
elif ! grep -q 'oitDescriptorGeneration = vk.oitAttachmentGeneration' "$OIT_DESC" 2>/dev/null; then
  fail "OIT descriptors must sync oitDescriptorGeneration after rebind"
elif ! grep -q 'descriptor generation != attachment generation' "$OIT_PASS" 2>/dev/null; then
  fail "vk_oit_pass must gate on attachment/descriptor generation match"
elif ! grep -q 'oit_status' "$PROJECT_ROOT/renderers/vulkan/vk_transparency_route.c" 2>/dev/null; then
  fail "missing oit_status command"
elif ! grep -q 'clusterOob' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag" 2>/dev/null; then
  fail "oit_accum.frag missing Forward+ cluster OOB guard"
else
  pass "OIT resolve FB attachmentCount=1 + generation gate + oit_status + cluster OOB"
fi

echo ""
echo "Input mouse-look lifecycle (SDL3):"
SDL_IN="$PROJECT_ROOT/engine/platform/sdl/sdl_input.c"
SDL_GLW="$PROJECT_ROOT/engine/platform/sdl/sdl_glw.h"
if ! grep -q 'input_status' "$SDL_IN" 2>/dev/null; then
  fail "sdl_input.c missing input_status command"
elif ! grep -q 'pixel_width' "$SDL_GLW" 2>/dev/null; then
  fail "sdl_glw.h must separate logical window_* from pixel_*"
elif ! grep -q 'mouse_frac_x' "$SDL_IN" 2>/dev/null; then
  fail "sdl_input.c must accumulate float mouse deltas (HiDPI)"
elif ! grep -q 'IN_SetRelativeMouse' "$SDL_IN" 2>/dev/null; then
  fail "sdl_input.c must check relative mouse mode return value"
else
  pass "SDL mouse-look: input_status, logical/pixel sizes, float accum, relative mode"
fi

echo ""
echo "OIT resolve math + reversed-Z depth:"
OIT_RESOLVE="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/oit_resolve.frag"
OIT_PIPE="$PROJECT_ROOT/renderers/vulkan/vk_pipeline_helpers.c"
if [[ ! -f "$OIT_RESOLVE" ]]; then
  fail "oit_resolve.frag missing"
elif ! grep -q 'c_avg \* coverage + opaque \* revealage' "$OIT_RESOLVE" 2>/dev/null; then
  fail "oit_resolve.frag missing McGuire composite C_avg*(1-R)+C_bg*R"
elif grep -qE 'oit_result \+ opaque \* revealage|accum\.rgb / max\(accum\.a.*, 1e-5\);\s*out_color = vec4\(oit_result \+ opaque' "$OIT_RESOLVE" 2>/dev/null; then
  fail "oit_resolve.frag still uses broken C_avg + C_bg*R (missing coverage scale)"
elif ! grep -q 'VK_COMPARE_OP_GREATER_OR_EQUAL' "$OIT_PIPE" 2>/dev/null; then
  fail "vk_pipeline_helpers.c OIT depth missing reversed-Z GREATER_OR_EQUAL"
elif grep -q 'oit_accum.*LESS_OR_EQUAL\|depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL' "$OIT_PIPE" 2>/dev/null; then
  # Allow LESS only if not in OIT pipelines — flag if any LESS remains in this helper file's OIT section.
  if grep -n 'depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL' "$OIT_PIPE" 2>/dev/null | grep -q .; then
    fail "vk_pipeline_helpers.c still has LESS_OR_EQUAL depth (OIT must match reversed-Z)"
  fi
else
  pass "OIT: McGuire resolve + reversed-Z GREATER_OR_EQUAL depth"
fi

echo ""
echo "RTX demo: invViewProj uses Vulkan projection + render-target extent:"
RTX_C="$PROJECT_ROOT/renderers/vulkan/extensions/rtx/vk_rtx.c"
if [[ ! -f "$RTX_C" ]]; then
  fail "vk_rtx.c missing"
elif ! grep -q 'vk_get_projection_matrix_vk' "$RTX_C" 2>/dev/null; then
  fail "vk_rtx.c missing vk_get_projection_matrix_vk (depth reprojection must match main pass)"
elif ! grep -q 'vk_get_render_target_width' "$RTX_C" 2>/dev/null; then
  fail "vk_rtx.c missing vk_get_render_target_width (RT dispatch must match FBO depth size)"
else
  pass "vk_rtx.c uses Vulkan projection flip and render-target extent"
fi

echo ""
echo "Vulkan RTX BLAS/TLAS + hybrid frame path:"
RTX_ENT="$PROJECT_ROOT/renderers/vulkan/extensions/rtx/vk_rtx_entities.c"
if ! grep -q 'r_rtxTlasUpdate' "$RTX_C" 2>/dev/null; then
  fail "vk_rtx.c missing r_rtxTlasUpdate TLAS update path"
elif ! grep -q 'VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR' "$RTX_C" 2>/dev/null; then
  fail "vk_rtx.c missing TLAS UPDATE build mode"
elif ! grep -q 'rtx_status' "$RTX_C" 2>/dev/null; then
  fail "vk_rtx.c missing rtx_status command"
elif ! grep -q 'gl_InstanceCustomIndexEXT' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/rtx_demo.rchit" 2>/dev/null; then
  fail "rtx_demo.rchit missing instance-aware closest-hit"
elif [[ ! -f "$RTX_ENT" ]]; then
  fail "vk_rtx_entities.c missing"
elif ! grep -q 'vk_rtx_pack_iqm' "$RTX_ENT" 2>/dev/null; then
  fail "vk_rtx_entities.c missing IQM pack"
elif ! grep -q 'R_IQMSkinPositions' "$RTX_ENT" 2>/dev/null; then
  fail "vk_rtx_entities.c missing CPU-skinned IQM via R_IQMSkinPositions"
elif ! grep -q 'vk_rtx_pack_gltf' "$RTX_ENT" 2>/dev/null; then
  fail "vk_rtx_entities.c missing glTF pack"
elif ! grep -q 'R_GLTFSkinPositions' "$RTX_ENT" 2>/dev/null; then
  fail "vk_rtx_entities.c missing CPU-skinned glTF via R_GLTFSkinPositions"
elif ! grep -q 'proxySkinnedCount' "$RTX_ENT" 2>/dev/null; then
  fail "vk_rtx_entities.c missing proxySkinnedCount stats"
elif ! grep -q 'R_MDRSkinSurfacePositions' "$RTX_ENT" 2>/dev/null; then
  fail "vk_rtx_entities.c missing CPU-skinned MDR via R_MDRSkinSurfacePositions"
elif ! grep -q 'vk_rtx_pack_mdr' "$RTX_ENT" 2>/dev/null; then
  fail "vk_rtx_entities.c missing MDR pack"
elif ! grep -q 'vk_rtx_bind_entity_albedo_ssbo' "$RTX_C" 2>/dev/null; then
  fail "vk_rtx.c missing entity albedo SSBO bind"
elif ! grep -q 'EntityAlbedoSSBO' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_hit.glsl" 2>/dev/null; then
  fail "hybrid1_hit.glsl missing EntityAlbedoSSBO"
else
  pass "RTX world/entity BLAS + TLAS update + hybrid hit tint + skinned IQM/glTF/MDR pack + entity hit attrs + Hybrid1 quality wired"
fi

echo ""
echo "Surfel GI (GIBS) chocolate path:"
SGI_C="$PROJECT_ROOT/renderers/vulkan/extensions/rtx/vk_surfel_gi.c"
SGI_SPAWN="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/surfel_gi/surfel_spawn.comp"
SGI_UPDATE="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/surfel_gi/surfel_update.comp"
if [[ ! -f "$SGI_C" ]]; then
  fail "vk_surfel_gi.c missing"
elif ! grep -q 'r_surfelGi' "$SGI_C" 2>/dev/null; then
  fail "vk_surfel_gi.c missing r_surfelGi cvar"
elif ! grep -q 'surfel_gi_status' "$SGI_C" 2>/dev/null; then
  fail "vk_surfel_gi.c missing surfel_gi_status"
elif [[ ! -f "$SGI_SPAWN" || ! -f "$SGI_UPDATE" ]]; then
  fail "surfel_gi spawn/update shaders missing"
elif ! grep -q 'GL_EXT_ray_query' "$SGI_UPDATE" 2>/dev/null; then
  fail "surfel_update.comp missing GL_EXT_ray_query"
elif ! grep -q 'WorldAlbedoSSBO' "$SGI_UPDATE" 2>/dev/null; then
  fail "surfel_update.comp missing WorldAlbedoSSBO"
elif ! grep -q 'WorldNormalSSBO' "$SGI_UPDATE" 2>/dev/null; then
  fail "surfel_update.comp missing WorldNormalSSBO"
elif ! grep -q 'hybrid1_sampleHitNormal' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_hit.glsl" 2>/dev/null; then
  fail "hybrid1_hit.glsl missing hybrid1_sampleHitNormal"
elif ! grep -q 'vk_rtx_bind_world_normal_ssbo' "$PROJECT_ROOT/renderers/vulkan/extensions/rtx/vk_rtx.c" 2>/dev/null; then
  fail "vk_rtx.c missing world normal SSBO bind"
elif ! grep -q 'WorldNormalSSBO' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/pt_hit.rchit" 2>/dev/null; then
  fail "pt_hit.rchit missing WorldNormalSSBO"
elif [[ ! -f "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/surfel_gi/surfel_hash.comp" ]]; then
  fail "surfel_hash.comp missing"
elif ! grep -q 'vk_surfel_gi.c' "$PROJECT_ROOT/cmake/renderers/VulkanExtensionSources.cmake" 2>/dev/null; then
  fail "VulkanExtensionSources.cmake missing vk_surfel_gi.c"
else
  pass "Surfel GI + Hybrid1/pathtrace world normal SSBO + chocolate link wired"
fi

echo ""
echo "Path trace experiment (Phase C6): vk_pathtrace + shaders:"
PT_C="$IDTECH3_RENDERERS/vulkan/extensions/rtx/vk_pathtrace.c"
if [[ ! -f "$PT_C" ]]; then
  PT_C="$IDTECH3_RENDERERS/vulkan/vk_pathtrace.c"
fi
PT_RGEN="$IDTECH3_RENDERERS/vulkan/shaders/glsl/pt_mega.rgen"
PT_DENOISE="$IDTECH3_RENDERERS/vulkan/shaders/glsl/pt_denoise.comp"
PT_COMP="$IDTECH3_RENDERERS/vulkan/shaders/glsl/pt_composite.comp"
if [[ ! -f "$PT_C" ]]; then
  fail "vk_pathtrace.c missing"
elif ! grep -q 'pipeline_denoise' "$PT_C" 2>/dev/null; then
  fail "vk_pathtrace.c missing wired denoise pipeline"
elif ! grep -q 'r_pathtrace_composite' "$PT_C" 2>/dev/null; then
  fail "vk_pathtrace.c missing r_pathtrace_composite blend"
elif [[ ! -f "$PT_RGEN" || ! -f "$PT_DENOISE" || ! -f "$PT_COMP" ]]; then
  fail "pathtrace shaders missing (pt_mega / pt_denoise / pt_composite)"
else
  pass "pathtrace megakernel/wavefront + denoise + composite wired"
fi

echo "Hybrid Rendering 1 (Granja/Pereira): vk_hybrid1 + shaders:"
H1_C="$PROJECT_ROOT/renderers/vulkan/extensions/rtx/vk_hybrid1.c"
H1_TEMP="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_temporal.comp"
H1_ATR="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_atrous.comp"
if [[ ! -f "$H1_C" ]]; then
  fail "vk_hybrid1.c missing"
elif ! grep -q 'r_hybrid1_historyClamp' "$H1_C" 2>/dev/null; then
  fail "vk_hybrid1.c missing history clamp wiring"
elif ! grep -q 'hybrid1_separableBlur' "$H1_C" 2>/dev/null; then
  fail "vk_hybrid1.c missing separable blur path"
elif ! grep -q 'r_hybrid1_diffuse' "$H1_C" 2>/dev/null; then
  fail "vk_hybrid1.c missing diffuse channel"
elif ! grep -q 'HYBRID1_RefreshRtDescriptors' "$H1_C" 2>/dev/null; then
  fail "vk_hybrid1.c missing per-frame IBL/G-buffer descriptor refresh"
elif ! grep -q 'r_hybrid1_motion' "$H1_C" 2>/dev/null; then
  fail "vk_hybrid1.c missing motion-vector temporal path"
elif ! grep -q 'tlas_mode=' "$H1_C" 2>/dev/null; then
  fail "hybrid1_status must report tlas_mode="
elif ! grep -q 'vk_rtx_tlas_status' "$H1_C" 2>/dev/null; then
  fail "vk_hybrid1.c must call vk_rtx_tlas_status for UPDATE/REBUILD reason"
elif [[ ! -f "$H1_TEMP" || ! -f "$H1_ATR" ]]; then
  fail "hybrid1 temporal/atrous shaders missing"
elif [[ ! -f "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_diffuse.rgen" ]]; then
  fail "hybrid1 diffuse RT shaders missing"
else
  pass "hybrid1 SVGF shadow/spec/diffuse + IBL + separable atrous + TLAS status wired"
fi

echo ""
echo "Forward+ tile cull: MAX_LEGACY_PER_TILE vs VK_FP_MAX_PER_TILE (legacy tile SSBO stride):"
FP_C="$PROJECT_ROOT/renderers/vulkan/vk_forward_plus.c"
legacy_tile_sh="$(sed -n 's/^#define MAX_LEGACY_PER_TILE[[:space:]]*\([0-9][0-9]*\)u*.*$/\1/p' "$FP_COMP" | head -1)"
compact_tile_sh="$(sed -n 's/^#define MAX_COMPACT_PER_TILE[[:space:]]*\([0-9][0-9]*\)u*.*$/\1/p' "$FP_COMP" | head -1)"
max_tile_c="$(sed -n 's/^#define VK_FP_MAX_PER_TILE[[:space:]]*\([0-9][0-9]*\)u*.*$/\1/p' "$FP_C" | head -1)"
max_compact_c="$(sed -n 's/^#define VK_FP_MAX_COMPACT_PER_CLUSTER[[:space:]]*\([0-9][0-9]*\)u*.*$/\1/p' "$FP_C" | head -1)"
if [[ -z "$legacy_tile_sh" || -z "$max_tile_c" ]]; then
  fail "could not parse MAX_LEGACY_PER_TILE from forward_plus_tile_cull.comp or VK_FP_MAX_PER_TILE from vk_forward_plus.c"
elif [[ "$legacy_tile_sh" != "$max_tile_c" ]]; then
  fail "MAX_LEGACY_PER_TILE ($legacy_tile_sh) != VK_FP_MAX_PER_TILE ($max_tile_c) - compute vs host legacy tile layout disagree"
else
  pass "MAX_LEGACY_PER_TILE=$legacy_tile_sh matches VK_FP_MAX_PER_TILE"
fi
if [[ -z "$compact_tile_sh" || -z "$max_compact_c" ]]; then
  fail "could not parse MAX_COMPACT_PER_TILE from forward_plus_tile_cull.comp or VK_FP_MAX_COMPACT_PER_CLUSTER from vk_forward_plus.c"
elif [[ "$compact_tile_sh" != "$max_compact_c" ]]; then
  fail "MAX_COMPACT_PER_TILE ($compact_tile_sh) != VK_FP_MAX_COMPACT_PER_CLUSTER ($max_compact_c) - compact cluster layout disagree"
else
  pass "MAX_COMPACT_PER_TILE=$compact_tile_sh matches VK_FP_MAX_COMPACT_PER_CLUSTER"
fi

echo ""
echo "Forward+ tile cap: VK_FP_MIN_PER_TILE vs MAX_LEGACY_PER_TILE (shader slot layout):"
min_tile_c="$(sed -n 's/^#define VK_FP_MIN_PER_TILE[[:space:]]*\([0-9][0-9]*\)u*.*$/\1/p' "$FP_C" | head -1)"
if [[ -z "$min_tile_c" || -z "$legacy_tile_sh" ]]; then
  fail "could not parse VK_FP_MIN_PER_TILE from vk_forward_plus.c or MAX_LEGACY_PER_TILE from forward_plus_tile_cull.comp"
elif [[ "$min_tile_c" -gt "$legacy_tile_sh" ]]; then
  fail "VK_FP_MIN_PER_TILE ($min_tile_c) > MAX_LEGACY_PER_TILE ($legacy_tile_sh) - r_forwardPlusMaxPerTile range would be empty"
else
  pass "VK_FP_MIN_PER_TILE=$min_tile_c <= MAX_LEGACY_PER_TILE=$legacy_tile_sh"
fi

echo ""
TR_INIT_VK="$PROJECT_ROOT/renderers/vulkan/tr_init.c"
if ! grep -q 'vk_forward_plus_get_min_per_tile_cap' "$TR_INIT_VK" || ! grep -q 'vk_forward_plus_get_max_per_tile_cap' "$TR_INIT_VK"; then
  fail "vulkan/tr_init.c should use vk_forward_plus_get_*_per_tile_cap for r_forwardPlusMaxPerTile CheckRange"
else
  pass "r_forwardPlusMaxPerTile CheckRange wired to vk_forward_plus tile caps"
fi

echo ""
echo "PBR fragment spec: Forward+ shade constant_id vs vk_create_pipeline.c:"
GEN_FRAG="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl"
VK_PIPE="$PROJECT_ROOT/renderers/vulkan/vk_create_pipeline.c"
fp_cid="$(grep -F 'forward_plus_shade_strength' "$GEN_FRAG" | grep 'constant_id' | sed -n 's/.*constant_id[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*/\1/p' | head -1)"
fp_add="$(grep 'forward_plus_shade_strength' "$VK_PIPE" | grep 'ADD_FRAG_SPEC' | sed -n 's/.*ADD_FRAG_SPEC([[:space:]]*\([0-9][0-9]*\)[[:space:]]*,[[:space:]]*forward_plus_shade_strength.*/\1/p' | head -1)"
if [[ -z "$fp_cid" || -z "$fp_add" ]]; then
  fail "could not parse forward_plus_shade_strength constant_id from gen_frag.tmpl or ADD_FRAG_SPEC from vk_create_pipeline.c"
elif [[ "$fp_cid" != "$fp_add" ]]; then
  fail "Forward+ shade specialization mismatch: gen_frag.tmpl constant_id=$fp_cid vs vk_create_pipeline.c ADD_FRAG_SPEC=$fp_add"
else
  pass "forward_plus_shade_strength uses constant_id=$fp_cid (shader + pipeline agree)"
fi

echo ""
echo "Forward+ tile cull: sphere overlap uses viewport-derived tile pixels (not hard-coded 16px):"
if grep -q 'tileX \* 16u\|tileY \* 16u' "$FP_COMP" 2>/dev/null; then
  fail "forward_plus_tile_cull.comp must not hard-code 16px tiles; derive tilePx from viewport / tileGrid (see vk_forward_plus VK_FP_TILE_DIM)"
elif ! grep -q 'tilePxX' "$FP_COMP" 2>/dev/null || ! grep -q 'sphere_tile_overlap' "$FP_COMP" 2>/dev/null; then
  fail "forward_plus_tile_cull.comp expected tilePxX/tilePxY sphere_tile_overlap path"
else
  fp_dim="$(sed -n 's/^#define VK_FP_TILE_DIM[[:space:]]*\([0-9][0-9]*\)u*$/\1/p' "$FP_C" | head -1)"
  pass "forward_plus_tile_cull uses dynamic tile pixels (VK_FP_TILE_DIM=$fp_dim on host grid)"
fi

echo ""
echo "Forward+ PBR fragment: legacy tile SSBO stride goes through shared cluster helper:"
FP_CLUSTER_HELPER="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/cluster_light_list.glsl"
if ! grep -q 'Cluster_FetchLightIndex' "$GEN_FRAG" 2>/dev/null || \
   ! grep -q 'Cluster_FetchLightIndex' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/forward_plus_light_eval.glsl" 2>/dev/null; then
 fail "Forward+ fragment paths must fetch light indices through Cluster_FetchLightIndex"
elif [[ -z "$legacy_tile_sh" ]]; then
  fail "could not re-use MAX_LEGACY_PER_TILE from forward_plus_tile_cull.comp for cluster helper stride check"
elif ! grep -qE '(tileId|clusterId)[[:space:]]*\*[[:space:]]*(legacyMax|maxPerLegacy)' "$FP_CLUSTER_HELPER" 2>/dev/null; then
  fail "Cluster_FetchLightIndex must use tileId * legacyMax for legacy tile SSBO layout"
else
  pass "Forward+ fragment paths use Cluster_FetchLightIndex legacyMax stride (MAX_LEGACY_PER_TILE=$legacy_tile_sh)"
fi

echo ""
echo "Forward+ SSBO: light header + record vec4 counts (vk_forward_plus.c vs tile_cull.comp vs gen_frag.tmpl):"
rec_stride_f="$(sed -n 's/^#define VK_FP_RECORD_STRIDE *(sizeof(float) *\*[[:space:]]*\([0-9][0-9]*\)).*$/\1/p' "$FP_C" | head -1)"
hdr_f="$(sed -n 's/^#define VK_FP_HEADER_BYTES *(sizeof(float) *\*[[:space:]]*\([0-9][0-9]*\)).*$/\1/p' "$FP_C" | head -1)"
rec_vec4s="$(sed -n 's/^#define REC_VEC4S[[:space:]]*\([0-9][0-9]*\)u*$/\1/p' "$FP_COMP" | head -1)"
if [[ -z "$rec_stride_f" || -z "$hdr_f" || -z "$rec_vec4s" ]]; then
  fail "could not parse VK_FP_RECORD_STRIDE / VK_FP_HEADER_BYTES from vk_forward_plus.c or REC_VEC4S from forward_plus_tile_cull.comp"
else
  exp_stride=$(( rec_vec4s * 4 ))
  if [[ "$rec_stride_f" != "$exp_stride" ]]; then
    fail "VK_FP_RECORD_STRIDE packs $rec_stride_f floats but REC_VEC4S=$rec_vec4s implies ${exp_stride} (4 floats per vec4)"
  elif [[ "$hdr_f" != "8" ]]; then
    fail "VK_FP_HEADER_BYTES uses sizeof(float)*$hdr_f; expected 8 (two header vec4s: lights.data[0..1])"
  else
    pass "Forward+ light SSBO: header ${hdr_f}f, record ${rec_stride_f}f (=REC_VEC4S $rec_vec4s * 4)"
  fi
fi
fp_frag_uniq="$(grep -E '2u \+ li \* [0-9]+u' "$GEN_FRAG" 2>/dev/null | sed -n 's/.*\* \([0-9][0-9]*\)u.*/\1/p' | sort -u | tr '\n' ' ' | xargs)"
if [[ -z "$fp_frag_uniq" ]]; then
  fail "gen_frag.tmpl: missing Forward+ light base index pattern '2u + li * <N>u'"
elif [[ "$(grep -E '2u \+ li \* [0-9]+u' "$GEN_FRAG" 2>/dev/null | sed -n 's/.*\* \([0-9][0-9]*\)u.*/\1/p' | sort -u | wc -l | tr -d ' ')" != "1" ]]; then
  fail "gen_frag.tmpl: multiple distinct '2u + li * Nu' multipliers ($fp_frag_uniq) — Forward+ record layout drift"
elif [[ "$fp_frag_uniq" != "$rec_vec4s" ]]; then
  fail "gen_frag.tmpl light record stride multiplier ($fp_frag_uniq) != REC_VEC4S ($rec_vec4s)"
else
  pass "gen_frag.tmpl Forward+ light base uses li * ${fp_frag_uniq}u (matches REC_VEC4S)"
fi

echo ""
echo "Forward+ tile cull: push constants (depthCull) host vs compute shader:"
if ! grep -q 'depth_cull' "$FP_C" 2>/dev/null || ! grep -q 'depthCull' "$FP_COMP" 2>/dev/null; then
  fail "vk_forward_plus.c vk_fp_push_t and forward_plus_tile_cull.comp Push must both declare depth_cull / depthCull"
elif ! grep -q 'push\.depth_cull = use_depth_cull' "$FP_C" 2>/dev/null; then
  fail "vk_forward_plus_dispatch_tile_cull_internal must assign push.depth_cull from use_depth_cull"
elif ! grep -q 'binding = 3' "$FP_COMP" 2>/dev/null || ! grep -q 'depthTexture' "$FP_COMP" 2>/dev/null; then
  fail "forward_plus_tile_cull.comp must declare depthTexture on binding 3 for r_forwardPlusDepthCull"
elif ! grep -q 'binds\[3\].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER' "$FP_C" 2>/dev/null; then
  fail "vk_forward_plus_create_set_layout must include binding 3 depth sampler for compute"
else
  pass "Forward+ depth cull: push depthCull + binding 3 depth sampler wired"
fi

echo ""
echo "Forward+ specular / energy renorm + HDR/AgX honesty:"
GEN_FRAG_TMPL="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl"
GAMMA_FRAG="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/gamma.frag"
VK_DEVICE="$PROJECT_ROOT/renderers/vulkan/vk_device.c"
if ! grep -q 'const int depth_r = 8' "$GAMMA_FRAG" 2>/dev/null ||
   ! grep -q 'const int depth_g = 8' "$GAMMA_FRAG" 2>/dev/null ||
   ! grep -q 'const int depth_b = 8' "$GAMMA_FRAG" 2>/dev/null; then
  fail "gamma.frag display-depth specialization defaults must be bit counts (8), not quantization levels (255)"
elif ! grep -q 'levels = exp2( bits ).*1.0' "$GAMMA_FRAG" 2>/dev/null; then
  fail "gamma.frag dithering must convert channel bit counts to (2^bits)-1 quantization intervals"
else
  pass "Display dither uses safe 8-bit defaults and 255 quantization intervals"
fi
if ! grep -q 'r_forwardPlusSpecularStrength' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_forwardPlusSpecularStrength cvar"
elif ! grep -q 'r_forwardPlusEnergyRenorm' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_forwardPlusEnergyRenorm cvar"
elif ! grep -q 'pbrForwardPlus\[2\]' "$PROJECT_ROOT/renderers/vulkan/tr_shade.c" 2>/dev/null; then
  fail "tr_shade.c must push specular strength via pbrForwardPlus[2]"
elif ! grep -q 'sceneNearest' "$FP_COMP" 2>/dev/null; then
  fail "forward_plus_tile_cull.comp should use multi-probe sceneNearest depth cull"
elif ! grep -q 'spotFrustumTileCull\|spot_frustum_tile_overlap' "$FP_COMP" 2>/dev/null; then
  fail "forward_plus_tile_cull.comp must implement spotFrustumTileCull for linear lights"
elif ! grep -q 'lightVolumeDepthCull' "$FP_COMP" 2>/dev/null; then
  fail "forward_plus_tile_cull.comp must implement lightVolumeDepthCull (volume Z vs sceneNearest)"
elif ! grep -q 'forwardPlusHiZProbePad' "$FP_COMP" 2>/dev/null || ! grep -q 'uint hiZ' "$FP_COMP" 2>/dev/null; then
  fail "forward_plus_tile_cull.comp must implement forwardPlusHiZProbePad / hiZ push"
elif ! grep -q 'r_forwardPlusHiZ' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_forwardPlusHiZ cvar"
elif ! grep -q 'r_forwardPlusHiZPyramid' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_forwardPlusHiZPyramid cvar"
elif ! grep -q 'aliases to 32-bit HDR' "$VK_DEVICE" 2>/dev/null; then
  fail "vk_device.c must honestly alias r_hdr 3 to 32-bit HDR"
elif ! grep -q 'toneMapParams0' "$GAMMA_FRAG" 2>/dev/null || ! grep -q 'Tonemap_AgX' "$GAMMA_FRAG" 2>/dev/null; then
  fail "gamma.frag Tonemap_AgX must use toneMapParams grade knobs"
elif ! grep -q 'agxStrength\|invWhite\|highlightDesat' "$GAMMA_FRAG" 2>/dev/null; then
  fail "gamma.frag Tonemap_AgX should consume toe/shoulder/whitePoint/desat"
elif ! grep -q 'default \*\*0\*\*' "$PROJECT_ROOT/docs/FORWARD_PLUS_PIPELINE_AUDIT.md" 2>/dev/null; then
  fail "FORWARD_PLUS_PIPELINE_AUDIT.md should document r_forwardPlusEnergyRenorm default **0**"
elif ! grep -q 'AgX = 4\|AgX (\`4\`)' "$PROJECT_ROOT/docs/samples/renderer_regression/scenes/05_postfx.md" 2>/dev/null; then
  fail "05_postfx.md must document AgX as r_tonemap 4 (Filmic is 3)"
else
  pass "Forward+ specular/renorm cvars + spot frustum + light-volume depth + AgX grade knobs"
fi

echo ""
echo "Forward+ vk_hiz pyramid sampling bridge:"
VK_HIZ_H="$PROJECT_ROOT/renderers/vulkan/vk_hiz.h"
VK_HIZ_C="$PROJECT_ROOT/renderers/vulkan/vk_hiz.c"
if ! grep -q 'vk_hiz_get_pyramid_sample_info' "$VK_HIZ_H" "$VK_HIZ_C" "$FP_C" 2>/dev/null; then
  fail "vk_hiz pyramid sample-info API must be exposed and consumed by Forward+"
elif ! grep -q 'binding = 9' "$FP_COMP" 2>/dev/null || ! grep -q 'dstBinding = 9' "$FP_C" 2>/dev/null; then
  fail "Forward+ tile cull must bind vk_hiz pyramid on descriptor binding 9"
elif ! grep -q 'uint hizPyramid' "$FP_COMP" 2>/dev/null || ! grep -q 'push.hiz_pyramid' "$FP_C" 2>/dev/null; then
  fail "Forward+ tile cull must gate pyramid sampling with push.hiz_pyramid"
elif ! grep -q 'r_forwardPlusHiZPyramid' "$FP_C" 2>/dev/null; then
  fail "Forward+ tile cull must gate real pyramid sampling with r_forwardPlusHiZPyramid"
elif ! grep -q 'uint hizLevels' "$FP_COMP" 2>/dev/null || ! grep -q 'push.hiz_levels' "$FP_C" 2>/dev/null; then
  fail "Forward+ tile cull must clamp pyramid LOD with live vk_hiz mip count"
elif ! grep -q 'textureLod(hizPyramidTexture' "$FP_COMP" 2>/dev/null; then
  fail "forward_plus_tile_cull.comp must sample vk_hiz pyramid with textureLod"
elif ! grep -q 'pyramidDispatch' "$FP_C" 2>/dev/null; then
  fail "cluster_status must report whether Forward+ will actually dispatch with vk_hiz pyramid sampling"
else
  pass "Forward+ can bind and optionally sample vk_hiz pyramid for depth cull"
fi

echo ""
echo "Day/night real-time world lighting:"
DAY_NIGHT_C="$PROJECT_ROOT/renderers/vulkan/vk_day_night.c"
DAY_NIGHT_H="$PROJECT_ROOT/renderers/vulkan/vk_day_night.h"
DAY_NIGHT_CFG="$PROJECT_ROOT/config/vulkan_overlay_day_night.cfg"
if [[ ! -f "$DAY_NIGHT_C" || ! -f "$DAY_NIGHT_H" ]]; then
  fail "vk_day_night module must exist"
elif ! grep -q 'r_dayNight' "$DAY_NIGHT_C" 2>/dev/null || ! grep -q 'r_dayNightUseRealTime' "$DAY_NIGHT_C" 2>/dev/null; then
  fail "day/night must expose real-time world-lighting cvars"
elif ! grep -q 'tr.sunDirection' "$DAY_NIGHT_C" 2>/dev/null || ! grep -q 'tr.sunLight' "$DAY_NIGHT_C" 2>/dev/null; then
  fail "day/night must drive canonical tr.sunDirection/tr.sunLight"
elif ! grep -q 'ri.Com_RealTime' "$DAY_NIGHT_C" 2>/dev/null || ! grep -q 'ri.Milliseconds' "$DAY_NIGHT_C" 2>/dev/null; then
  fail "day/night must support wall-clock and accelerated cycle timing"
elif ! grep -q 'vk_weather_direct_sun_factor' "$PROJECT_ROOT/renderers/vulkan/vk_weather.h" "$PROJECT_ROOT/renderers/vulkan/vk_weather.c" "$DAY_NIGHT_C" 2>/dev/null; then
  fail "weather must publish and day/night must consume direct sun dimming"
elif ! grep -q 'vk_weather_shadow_factor' "$PROJECT_ROOT/renderers/vulkan/vk_weather.h" "$PROJECT_ROOT/renderers/vulkan/vk_weather.c" "$DAY_NIGHT_C" 2>/dev/null; then
  fail "weather must publish and day/night must consume shadow dimming"
elif ! grep -q 'Weather feeds canonical day/night sun radiance' "$PROJECT_ROOT/renderers/vulkan/vk_frame_submit.c" 2>/dev/null; then
  fail "weather must update before day/night prepares canonical sun lighting"
elif ! grep -q 'vk_day_night_shadow_factor' "$DAY_NIGHT_H" "$DAY_NIGHT_C" 2>/dev/null; then
  fail "day/night must expose an effective shadow factor"
elif ! grep -q 'vk_day_night_shadow_factor' "$PROJECT_ROOT/renderers/vulkan/tr_backend.c" "$PROJECT_ROOT/renderers/vulkan/tr_shade.c" "$PROJECT_ROOT/renderers/vulkan/vk_deferred_gbuffer.c" "$PROJECT_ROOT/renderers/vulkan/vk_view_state.c" 2>/dev/null; then
  fail "day/night shadow factor must drive CSM production and raster/deferred/OIT consumers"
elif ! grep -q 'vk_day_night_begin_frame' "$PROJECT_ROOT/renderers/vulkan/vk_frame_submit.c" 2>/dev/null; then
  fail "day/night must update before renderer frame consumers"
elif ! grep -q 'vk_day_night_on_world_load' "$PROJECT_ROOT/renderers/vulkan/tr_bsp.c" 2>/dev/null; then
  fail "day/night must capture authored map sun after world load"
elif ! grep -q 'daynight_status' "$DAY_NIGHT_C" 2>/dev/null; then
  fail "day/night must expose daynight_status diagnostics"
elif [[ ! -f "$DAY_NIGHT_CFG" ]] || ! grep -q 'seta r_dayNight 1' "$DAY_NIGHT_CFG" 2>/dev/null || ! grep -q 'r_dayNightMoonShadow' "$DAY_NIGHT_CFG" 2>/dev/null; then
  fail "day/night overlay config missing"
else
  pass "Day/night world lighting drives canonical sun and shadow strength from real time"
fi

echo ""
echo "Volumetric fog compute: VDB bindings 17-18 + params (host vs volumetric_fog.comp):"
VF_COMP="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/volumetric/volumetric_fog.comp"
VF_FRAG="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/volumetric/volumetric_fog.frag"
VK_INIT="$PROJECT_ROOT/renderers/vulkan/vk_init_device.c"
VK_VOL_P="$PROJECT_ROOT/renderers/vulkan/vk_volumetric_params.h"
if [[ ! -f "$VF_COMP" ]]; then
  fail "missing volumetric_fog.comp"
elif ! grep -q 'binding = 17' "$VF_COMP" 2>/dev/null || ! grep -q 'vdbFogDensity' "$VF_COMP" 2>/dev/null; then
  fail "volumetric_fog.comp must declare sampler3D vdbFogDensity on binding 17"
elif ! grep -q 'binding = 18' "$VF_COMP" 2>/dev/null || ! grep -q 'vdbFogMajorant' "$VF_COMP" 2>/dev/null; then
  fail "volumetric_fog.comp must declare sampler3D vdbFogMajorant on binding 18"
elif ! grep -q 'compute_bindings\[18\]' "$VK_INIT" 2>/dev/null || ! grep -q 'compute_bindings\[17\].binding = 17' "$VK_INIT" 2>/dev/null; then
  fail "vk_init_device.c volumetric compute layout must include bindings 17-18 (19 bindings total)"
elif ! grep -q 'vdbParams\[4\]' "$VK_VOL_P" 2>/dev/null || ! grep -q 'vdbWorldMin\[4\]' "$VK_VOL_P" 2>/dev/null; then
  fail "volumetric_params_t must include vdbParams / vdbWorldMin / vdbWorldMax for r_vdbFog"
elif [[ ! -f "$VF_FRAG" ]] || ! grep -q 'integrateVdbWoodcockFog' "$VF_FRAG" 2>/dev/null; then
  fail "volumetric_fog.frag must implement integrateVdbWoodcockFog for integration mode 3"
elif [[ ! -f "$PROJECT_ROOT/renderers/vulkan/vk_nanovdb_decode.c" ]] || \
     ! grep -q 'VDB_NanoVDB_DecodeToDense' "$PROJECT_ROOT/renderers/vulkan/vk_nanovdb_decode.c" 2>/dev/null; then
  fail "vk_nanovdb_decode.c must implement VDB_NanoVDB_DecodeToDense for .nvdb voxel fill"
else
  pass "VDB volumetric fog: bindings 17-18 + Woodcock + NanoVDB decode present"
fi

echo ""
echo "VDB console workflow (vdb_load / upload / bind_fog):"
VK_VDB_C="$PROJECT_ROOT/renderers/vulkan/vk_vdb.c"
if [[ ! -f "$VK_VDB_C" ]]; then
  fail "missing vk_vdb.c"
elif ! grep -q 'ri\.Cmd_AddCommand( "vdb_load"' "$VK_VDB_C" || \
     ! grep -q 'ri\.Cmd_AddCommand( "vdb_bind_fog"' "$VK_VDB_C" || \
     ! grep -q 'ri\.Cmd_AddCommand( "vdb_rebuild_majorant"' "$VK_VDB_C" || \
     ! grep -q 'VDB_FrameUpdate' "$VK_VDB_C" || \
     ! grep -q 'vk_update_volumetric_descriptors' "$VK_VDB_C"; then
  fail "vk_vdb.c must register vdb commands, VDB_FrameUpdate, and refresh volumetric descriptors on upload"
else
  pass "VDB console commands registered in vk_vdb.c"
fi

echo ""
echo "Optional Tiled Map Editor submodule (GPL-2.0, not linked into engine):"
GITMODULES="$PROJECT_ROOT/.gitmodules"
if [[ ! -f "$GITMODULES" ]]; then
  fail "missing .gitmodules"
elif ! grep -qE '^\[submodule "tools/tiled"\]' "$GITMODULES" || \
     ! grep -qE '^\s*path = tools/tiled' "$GITMODULES" || \
     ! grep -q 'github.com/mapeditor/tiled' "$GITMODULES"; then
  fail ".gitmodules must register tools/tiled -> mapeditor/tiled"
else
  pass ".gitmodules registers tools/tiled (mapeditor/tiled)"
fi
if ! git -C "$PROJECT_ROOT" rev-parse --git-dir &>/dev/null; then
  fail "not a git checkout (cannot verify tools/tiled gitlink)"
elif ! git -C "$PROJECT_ROOT" ls-tree HEAD tools/tiled 2>/dev/null | grep -q '160000'; then
  fail "tools/tiled must be a submodule gitlink (mode 160000) at HEAD"
else
  pass "tools/tiled submodule gitlink present at HEAD"
fi
if [[ ! -f "$PROJECT_ROOT/docs/TILED.md" ]]; then
  fail "missing docs/TILED.md"
else
  pass "docs/TILED.md present"
fi

echo ""
echo "Vulkan temporal: reset bitmask vs reason_string / log table:"
VK_TEMP_H="$PROJECT_ROOT/renderers/vulkan/vk_temporal.h"
VK_TEMP_C="$PROJECT_ROOT/renderers/vulkan/vk_temporal.c"
# One line per 1u<<N reset flag in the public enum (excludes VK_TEMPORAL_RESET_NONE = 0).
n_enum="$(grep -E '^[[:space:]]*VK_TEMPORAL_RESET_[A-Z0-9_]+[[:space:]]*=[[:space:]]*1u[[:space:]]*<<' "$VK_TEMP_H" 2>/dev/null | wc -l | tr -d ' ')"
n_case="$(grep -E '^[[:space:]]*case VK_TEMPORAL_RESET_' "$VK_TEMP_C" 2>/dev/null | wc -l | tr -d ' ')"
n_arr="$(awk '/knownReasons\[\][[:space:]]*=[[:space:]]*\{/,/\}[[:space:]]*;/' "$VK_TEMP_C" 2>/dev/null | grep -c 'VK_TEMPORAL_RESET_' | tr -d ' ')"
if [[ -z "$n_enum" || "$n_enum" -eq 0 ]]; then
  fail "could not count VK_TEMPORAL_RESET_* 1u<< lines in vk_temporal.h"
elif [[ "$n_enum" != "$n_case" ]]; then
  fail "vk_temporal reset count mismatch: enum 1u<< lines=$n_enum vs switch cases=$n_case in vk_temporal.c"
elif [[ "$n_enum" != "$n_arr" ]]; then
  fail "vk_temporal reset count mismatch: enum 1u<< lines=$n_enum vs knownReasons[] entries=$n_arr in vk_temporal.c"
else
  pass "vk_temporal: $n_enum reset flags, $n_case switch cases, $n_arr knownReasons entries"
fi

echo ""
echo "Vegetation wind dispatch ordering (staging must be populated before compute):"
TR_SHADE="$PROJECT_ROOT/renderers/vulkan/tr_shade.c"
VK_FRAME_SUBMIT="$PROJECT_ROOT/renderers/vulkan/vk_frame_submit.c"
if awk '
  /PostFX_VegWind_IsEnabled\(\) && tess\.shader && \( tess\.shader->surfaceFlags & SURF_VEGETATION \)/ { guard=1 }
  /vk_vegetation_wind_prepare_draw\(\);/ { prepare=1 }
  END { exit !(guard && prepare) }
' "$TR_SHADE"; then
  pass "tr_shade.c prepares vegetation wind from SURF_VEGETATION batches before draw"
else
  fail "tr_shade.c is missing SURF_VEGETATION-gated vk_vegetation_wind_prepare_draw()"
fi
if grep -q 'vk_vegetation_wind_prepare_draw();' "$VK_FRAME_SUBMIT"; then
  fail "vk_frame_submit.c should not prepare vegetation wind at frame start"
else
  pass "vk_frame_submit.c has no direct veg-wind prepare call"
fi

echo ""
echo "Vulkan mesh-shader extension gating (startup safety):"
TR_INIT_VK="$PROJECT_ROOT/renderers/vulkan/tr_init.c"
VK_INSTANCE="$PROJECT_ROOT/renderers/vulkan/vk_instance.c"
if grep -Fq 'r_vk_meshShaderNV = ri.Cvar_Get( "r_vk_meshShaderNV", "0"' "$TR_INIT_VK"; then
  pass "r_vk_meshShaderNV cvar registered with default 0"
else
  fail "missing r_vk_meshShaderNV cvar registration with default 0 in tr_init.c"
fi

if grep -Fq 'vk.meshShaderNV = qfalse;' "$VK_INSTANCE"; then
  pass "vk.meshShaderNV reset each device creation"
else
  fail "vk.meshShaderNV is not reset before extension selection"
fi

if grep -Fq 'if ( nvMeshShader && r_vk_meshShaderNV && r_vk_meshShaderNV->integer &&' "$VK_INSTANCE"; then
  pass "VK_NV_mesh_shader enable path gated by support + cvar"
else
  fail "VK_NV_mesh_shader enable path is missing support/cvar gating"
fi

if grep -Fq 'device_extension_count < ARRAY_LEN( device_extension_list ) ) {' "$VK_INSTANCE"; then
  pass "VK_NV_mesh_shader enable path guards extension-list capacity"
else
  fail "VK_NV_mesh_shader enable path missing extension-list capacity guard"
fi

if grep -Fq 'mesh_shader_features_nv.meshShader = VK_TRUE;' "$VK_INSTANCE" && \
   grep -Fq 'mesh_shader_features_nv.pNext = (void *)(uintptr_t)device_desc.pNext;' "$VK_INSTANCE"; then
  pass "mesh-shader feature struct is chained to current device_desc.pNext head"
else
  fail "mesh-shader feature chain setup is missing required pNext/meshShader assignments"
fi

echo ""
echo "glTF topo: GLTF_GPU_TOPO_WORDS_PER_VERT macro (tr_gltf_topo.h vs gen_vert.tmpl formula):"
TOPO_H="$PROJECT_ROOT/renderers/vulkan/tr_gltf_topo.h"
if ! grep 'GLTF_GPU_TOPO_WORDS_PER_VERT' "$TOPO_H" | grep -q '1.*+.*GLTF_GPU_ADJ_TRIS_MAX'; then
  fail "tr_gltf_topo.h: GLTF_GPU_TOPO_WORDS_PER_VERT must expand to (1 + GLTF_GPU_ADJ_TRIS_MAX)"
elif ! grep -q 'const uint GLTF_GPU_TOPO_WORDS_PER_VERT = 1u + GLTF_GPU_ADJ_TRIS_MAX' "$GEN_VERT"; then
  fail "gen_vert.tmpl: GLTF_GPU_TOPO_WORDS_PER_VERT must be 1u + GLTF_GPU_ADJ_TRIS_MAX (match tr_gltf_topo.h)"
else
  pass "GLTF_GPU_TOPO_WORDS_PER_VERT = 1 + ADJ (C header + GLSL)"
fi

echo ""
echo "glTF topo: GLTF_GPU_PULL_UINTS_PER_VERT (tr_gltf_topo.h vs gen_vert.tmpl):"
pull_c="$(sed -n 's/^#define GLTF_GPU_PULL_UINTS_PER_VERT[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$TOPO_H" | head -1)"
pull_glsl="$(grep -E 'const uint GLTF_GPU_PULL_UINTS_PER_VERT' "$GEN_VERT" | head -1 | sed -n 's/.*GLTF_GPU_PULL_UINTS_PER_VERT = \([0-9][0-9]*\)u.*/\1/p')"
if [[ -z "$pull_c" || -z "$pull_glsl" ]]; then
  fail "could not parse GLTF_GPU_PULL_UINTS_PER_VERT from tr_gltf_topo.h or gen_vert.tmpl"
elif [[ "$pull_c" != "$pull_glsl" ]]; then
  fail "GLTF_GPU_PULL_UINTS_PER_VERT mismatch: C=$pull_c GLSL=$pull_glsl"
else
  pass "GLTF_GPU_PULL_UINTS_PER_VERT=$pull_c (C + GLSL)"
fi

echo ""
echo "Raster Ultra 1.11 Reference Lab:"
if [[ ! -f "$PROJECT_ROOT/scripts/raster_ultra_1_11_check.sh" ]]; then
  fail "missing scripts/raster_ultra_1_11_check.sh"
elif ! bash "$PROJECT_ROOT/scripts/raster_ultra_1_11_check.sh"; then
  fail "raster_ultra_1_11_check.sh failed"
else
  pass "Raster Ultra 1.11 reference lab static contract"
fi

echo ""
echo "Raster Ultra 1.12 Frequency-Aware:"
if [[ ! -f "$PROJECT_ROOT/scripts/raster_ultra_1_12_check.sh" ]]; then
  fail "missing scripts/raster_ultra_1_12_check.sh"
elif ! bash "$PROJECT_ROOT/scripts/raster_ultra_1_12_check.sh"; then
  fail "raster_ultra_1_12_check.sh failed"
else
  pass "Raster Ultra 1.12 frequency-aware static contract"
fi

echo ""
echo "Raster Ultra 1.14 Terrain + Vegetation:"
if [[ ! -f "$PROJECT_ROOT/scripts/raster_ultra_1_14_check.sh" ]]; then
  fail "missing scripts/raster_ultra_1_14_check.sh"
elif ! bash "$PROJECT_ROOT/scripts/raster_ultra_1_14_check.sh"; then
  fail "raster_ultra_1_14_check.sh failed"
else
  pass "Raster Ultra 1.14 terrain/biome/veg static contract"
fi

echo ""
echo "Raster Ultra 2.0 Production Hardening:"
if [[ ! -f "$PROJECT_ROOT/scripts/raster_ultra_2_0_check.sh" ]]; then
  fail "missing scripts/raster_ultra_2_0_check.sh"
elif ! bash "$PROJECT_ROOT/scripts/raster_ultra_2_0_check.sh"; then
  fail "raster_ultra_2_0_check.sh failed"
else
  pass "Raster Ultra 2.0 frame-contract static gate"
fi
if [[ ! -f "$PROJECT_ROOT/scripts/raster_ultra_2_1_check.sh" ]]; then
  fail "missing scripts/raster_ultra_2_1_check.sh"
elif ! bash "$PROJECT_ROOT/scripts/raster_ultra_2_1_check.sh"; then
  fail "raster_ultra_2_1_check.sh failed"
else
  pass "Raster Ultra 2.1 spatial AA static gate"
fi
if [[ ! -f "$PROJECT_ROOT/scripts/cinematic_engine_1_0_check.sh" ]]; then
  fail "missing scripts/cinematic_engine_1_0_check.sh"
elif ! bash "$PROJECT_ROOT/scripts/cinematic_engine_1_0_check.sh"; then
  fail "cinematic_engine_1_0_check.sh failed"
else
  pass "Cinematic Engine Platform 1.0 environment-slice static gate"
fi

echo ""
if [ -n "${GAME_BASE:-}" ]; then
  echo "Optional game base: $GAME_BASE"
  ASSETS_LIST="${GAME_ASSETS_LIST:-$PROJECT_ROOT/docs/samples/renderer_regression/OPTIONAL_GAME_ASSETS.txt}"
  req=0
  while IFS= read -r line || [ -n "$line" ]; do
    line="${line%$'\r'}"
    [[ -z "$line" || "$line" =~ ^# ]] && continue
    req=$((req + 1))
    f="$GAME_BASE/$line"
    if [ -f "$f" ]; then
      pass "game asset: $line"
    else
      fail "missing game asset: $line (expected under GAME_BASE)"
    fi
  done < "$ASSETS_LIST"
  if [ "$req" -eq 0 ]; then
    echo "  (no uncommented paths in OPTIONAL_GAME_ASSETS.txt - add BSP paths to enforce)"
  fi
else
  echo "GAME_BASE unset - skipping optional BSP checks (uncomment paths in OPTIONAL_GAME_ASSETS.txt to enforce in CI)."
fi

echo ""
echo "=== Summary ==="
echo "  Passed: $PASS  Failed: $FAIL"
if [ "$FAIL" -gt 0 ]; then
  echo "RENDERER REGRESSION CHECK FAILED" >&2
  exit 1
fi
echo "RENDERER REGRESSION CHECK PASSED"
exit 0
