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
echo "TAA shader: neighborhoodMinMax must run before history clamp:"
TAA_FRAG="$PROJECT_ROOT/renderers/vulkan/shaders/glsl/taa.frag"
if ! grep -Eq 'neighborhoodMinMax\( (uv|sampleUV), mn, mx, avg \)' "$TAA_FRAG" 2>/dev/null; then
  fail "taa.frag missing neighborhoodMinMax() call (history AABB clamp would be undefined)"
else
  pass "taa.frag calls neighborhoodMinMax before history clamp"
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
  fail "tr_render_mode_vk.c should latch deferred lighting (r_forwardPlusShade 0)"
else
  pass "R_ApplyRenderModeLatch wired (tr_render_mode_vk.c, R_Init, vk_forward_plus)"
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
elif ! grep -q 'vk_entity_note_motion_reliability' "$PROJECT_ROOT/renderers/vulkan/vk_view_state.c" 2>/dev/null; then
  fail "vk_view_state.c missing per-entity motion reliability notes"
else
  pass "TAA uses per-frame history confidence; per-entity motion policy wired"
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
elif ! grep -q 'pc.additive' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/deferred_lighting.comp" 2>/dev/null; then
  fail "deferred_lighting.comp missing additive composite path"
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
elif ! grep -q 'pc.specular' "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/deferred_lighting.comp" 2>/dev/null; then
  fail "deferred_lighting.comp missing specular toggle"
else
  pass "deferred lighting compute + composite wired"
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
echo "Engine-native sprite props (misc_billboard / misc_flipbook / misc_imposter):"
SP_C="$PROJECT_ROOT/renderers/vulkan/tr_sprite_props.c"
TR_TYPES="$PROJECT_ROOT/renderers/common/tr_types.h"
if ! grep -q 'misc_billboard' "$SP_C" 2>/dev/null; then
  fail "tr_sprite_props.c missing misc_billboard parse"
elif ! grep -q 'misc_flipbook' "$SP_C" 2>/dev/null; then
  fail "tr_sprite_props.c missing misc_flipbook parse"
elif ! grep -q 'misc_imposter' "$SP_C" 2>/dev/null; then
  fail "tr_sprite_props.c missing misc_imposter parse"
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
elif ! grep -q 'SV_EngineSprites_LoadMap' "$PROJECT_ROOT/runtime/server/sv_init.c" 2>/dev/null; then
  fail "sv_init.c should load map sprite shaders on CM_LoadMap"
elif ! grep -q 'SV_EngineSprites_SpawnMapEntities' "$PROJECT_ROOT/runtime/server/sv_init.c" 2>/dev/null; then
  fail "sv_init.c should spawn map sprite snapshot ents after game init"
elif ! grep -q 'CS_ENGINE_SPRITE_META' "$PROJECT_ROOT/runtime/game/bg_public.h" 2>/dev/null; then
  fail "bg_public.h missing CS_ENGINE_SPRITE_META"
elif ! grep -q 'G_ENGINE_SPRITE_SPAWN' "$PROJECT_ROOT/runtime/game/g_public.h" 2>/dev/null; then
  fail "g_public.h missing G_ENGINE_SPRITE_SPAWN game trap"
elif ! grep -q 'SV_EngineSprite_SpawnFromDef' "$PROJECT_ROOT/runtime/server/sv_engine_sprites.c" 2>/dev/null; then
  fail "sv_engine_sprites.c missing runtime spawn helper"
elif ! grep -q 'registerTable(L, "Sprites"' "$PROJECT_ROOT/runtime/game/g_lua_bindings.c" 2>/dev/null; then
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
elif ! grep -q 'registerTable(L, "Decals"' "$PROJECT_ROOT/runtime/game/g_lua_bindings.c" 2>/dev/null; then
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
  fail "vk_rtx_entities.c missing bind-pose IQM pack"
elif ! grep -q 'vk_rtx_pack_gltf_static' "$RTX_ENT" 2>/dev/null; then
  fail "vk_rtx_entities.c missing static glTF pack"
elif ! grep -q 'proxySkinnedCount' "$RTX_ENT" 2>/dev/null; then
  fail "vk_rtx_entities.c missing proxySkinnedCount stats"
else
  pass "RTX world/entity BLAS + TLAS update + hybrid hit tint + IQM/glTF pack wired"
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
elif ! grep -q 'vk_surfel_gi.c' "$PROJECT_ROOT/cmake/renderers/VulkanExtensionSources.cmake" 2>/dev/null; then
  fail "VulkanExtensionSources.cmake missing vk_surfel_gi.c"
else
  pass "Surfel GI spawn/update + ray query + chocolate link wired"
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
elif [[ ! -f "$H1_TEMP" || ! -f "$H1_ATR" ]]; then
  fail "hybrid1 temporal/atrous shaders missing"
elif [[ ! -f "$PROJECT_ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_diffuse.rgen" ]]; then
  fail "hybrid1 diffuse RT shaders missing"
else
  pass "hybrid1 SVGF shadow/spec/diffuse + IBL + separable atrous wired"
fi

echo ""
echo "Forward+ tile cull: MAX_PER_TILE vs VK_FP_MAX_PER_TILE (tile SSBO stride):"
FP_C="$PROJECT_ROOT/renderers/vulkan/vk_forward_plus.c"
max_tile_sh="$(sed -n 's/^#define MAX_PER_TILE[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$FP_COMP" | head -1)"
max_tile_c="$(sed -n 's/^#define VK_FP_MAX_PER_TILE[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$FP_C" | head -1)"
if [[ -z "$max_tile_sh" || -z "$max_tile_c" ]]; then
  fail "could not parse MAX_PER_TILE from forward_plus_tile_cull.comp or VK_FP_MAX_PER_TILE from vk_forward_plus.c"
elif [[ "$max_tile_sh" != "$max_tile_c" ]]; then
  fail "MAX_PER_TILE ($max_tile_sh) != VK_FP_MAX_PER_TILE ($max_tile_c) - compute vs host tile layout disagree"
else
  pass "MAX_PER_TILE=$max_tile_sh matches VK_FP_MAX_PER_TILE"
fi

echo ""
echo "Forward+ tile cap: VK_FP_MIN_PER_TILE vs MAX_PER_TILE (shader slot layout):"
min_tile_c="$(sed -n 's/^#define VK_FP_MIN_PER_TILE[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$FP_C" | head -1)"
if [[ -z "$min_tile_c" || -z "$max_tile_sh" ]]; then
  fail "could not parse VK_FP_MIN_PER_TILE from vk_forward_plus.c or MAX_PER_TILE from forward_plus_tile_cull.comp"
elif [[ "$min_tile_c" -gt "$max_tile_sh" ]]; then
  fail "VK_FP_MIN_PER_TILE ($min_tile_c) > MAX_PER_TILE ($max_tile_sh) - r_forwardPlusMaxPerTile range would be empty"
else
  pass "VK_FP_MIN_PER_TILE=$min_tile_c <= MAX_PER_TILE=$max_tile_sh"
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
echo "Forward+ PBR fragment: tile SSBO stride (tileId * N) matches MAX_PER_TILE:"
fp_stride="$(grep -E 'tileId \* [0-9]+u' "$GEN_FRAG" 2>/dev/null | sed -n 's/.*tileId \* \([0-9][0-9]*\)u.*/\1/p' | head -1)"
if [[ -z "$fp_stride" ]]; then
  fail "gen_frag.tmpl: expected tileId * <N>u for Forward+ tile base (fp_tiles stride)"
elif [[ -z "$max_tile_sh" ]]; then
  fail "could not re-use MAX_PER_TILE from forward_plus_tile_cull.comp for gen_frag stride check"
elif [[ "$fp_stride" != "$max_tile_sh" ]]; then
  fail "gen_frag.tmpl tile stride uses * ${fp_stride}u but MAX_PER_TILE=$max_tile_sh - tile SSBO layout vs fragment disagree"
else
  pass "gen_frag.tmpl tile stride * ${fp_stride}u matches MAX_PER_TILE"
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
if ! grep -q 'r_forwardPlusSpecularStrength' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_forwardPlusSpecularStrength cvar"
elif ! grep -q 'r_forwardPlusEnergyRenorm' "$TR_INIT_VK" 2>/dev/null; then
  fail "tr_init.c missing r_forwardPlusEnergyRenorm cvar"
elif ! grep -q 'pbrForwardPlus\[2\]' "$PROJECT_ROOT/renderers/vulkan/tr_shade.c" 2>/dev/null; then
  fail "tr_shade.c must push specular strength via pbrForwardPlus[2]"
elif ! grep -q 'sceneNearest' "$FP_COMP" 2>/dev/null; then
  fail "forward_plus_tile_cull.comp should use multi-probe sceneNearest depth cull"
elif ! grep -q 'aliases to 32-bit HDR' "$VK_DEVICE" 2>/dev/null; then
  fail "vk_device.c must honestly alias r_hdr 3 to 32-bit HDR"
elif ! grep -q 'toneMapParams0' "$GAMMA_FRAG" 2>/dev/null || ! grep -q 'Tonemap_AgX' "$GAMMA_FRAG" 2>/dev/null; then
  fail "gamma.frag Tonemap_AgX must use toneMapParams grade knobs"
elif ! grep -q 'agxStrength\|invWhite\|highlightDesat' "$GAMMA_FRAG" 2>/dev/null; then
  fail "gamma.frag Tonemap_AgX should consume toe/shoulder/whitePoint/desat"
else
  pass "Forward+ specular/renorm cvars + tile probes + r_hdr 3 alias + AgX grade knobs"
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
