#!/usr/bin/env bash
# Wiring test: deferred G-buffer fill + lighting + composite (r_renderMode 1).
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

DGB="$(idtech3_file renderers/vulkan/vk_deferred_gbuffer.c src/renderers/vulkan/vk_deferred_gbuffer.c)"
LIT="$(idtech3_file renderers/vulkan/shaders/glsl/deferred_lighting.comp src/renderers/vulkan/shaders/glsl/deferred_lighting.comp)"
COMP="$(idtech3_file renderers/vulkan/shaders/glsl/deferred_lighting_composite.frag src/renderers/vulkan/shaders/glsl/deferred_lighting_composite.frag)"
GBUF_FILL="$(idtech3_file renderers/vulkan/shaders/glsl/deferred_gbuffer_fill.comp src/renderers/vulkan/shaders/glsl/deferred_gbuffer_fill.comp)"
GBUF_DEBUG="$(idtech3_file renderers/vulkan/shaders/glsl/deferred_gbuffer_debug.frag src/renderers/vulkan/shaders/glsl/deferred_gbuffer_debug.frag)"
ATTACH="$(idtech3_file renderers/vulkan/vk_attachments.c src/renderers/vulkan/vk_attachments.c)"
PIPE="$(idtech3_file renderers/vulkan/vk_create_pipeline.c src/renderers/vulkan/vk_create_pipeline.c)"
RPASS="$(idtech3_file renderers/vulkan/vk_render_pass.c src/renderers/vulkan/vk_render_pass.c)"
FBO="$(idtech3_file renderers/vulkan/vk_framebuffers.c src/renderers/vulkan/vk_framebuffers.c)"
GEN_FRAG="$(idtech3_file renderers/vulkan/shaders/glsl/gen_frag.tmpl src/renderers/vulkan/shaders/glsl/gen_frag.tmpl)"
TR_INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"
TR_BACKEND="$(idtech3_file renderers/vulkan/tr_backend.c src/renderers/vulkan/tr_backend.c)"
DEFERRED_CFG="$ROOT/config/deferred_vulkan.cfg"

check "$DEFERRED_CFG" 'seta r_renderMode 1' 'deferred profile selects mode 1'
check "$DEFERRED_CFG" 'seta r_deferredLighting 1' 'deferred profile enables deferred lighting'
check "$DEFERRED_CFG" 'seta r_forwardPlusShade 0' 'deferred profile disables Forward+ primary shade'
check "$DEFERRED_CFG" 'seta r_deferredAOCoupling 0.65' 'deferred profile enables AO-coupled dynamic lighting'
check "$DEFERRED_CFG" 'seta r_deferredSpecularStrength 1' 'deferred profile pins specular strength'
check "$DEFERRED_CFG" 'seta r_deferredDefaultRoughness 0.55' 'deferred profile sets fallback roughness'
check "$DEFERRED_CFG" 'seta r_taa 0' 'deferred profile disables TAA by default'
check "$DEFERRED_CFG" 'seta r_taaMotionVectors 0' 'deferred profile disables motion-vector TAA by default'
check "$DEFERRED_CFG" 'seta r_ext_smaa 0' 'deferred profile disables SMAA by default'
check "$DEFERRED_CFG" 'seta r_ext_fxaa 0' 'deferred profile disables FXAA by default'
check "$DEFERRED_CFG" 'seta r_postAaAfterBloom 0' 'deferred profile disables post-AA reapply by default'
check "$ROOT/scripts/compile_engine.sh" 'deferred_vulkan.cfg' 'release packaging ships deferred profile'
check "$TR_INIT" 'renderer_deferred_safe' 'deferred-safe recovery command registered'
check "$TR_INIT" 'r_deferredSpecular = ri.Cvar_Get' 'r_deferredSpecular cvar'
check "$TR_INIT" 'r_deferredSpecularStrength = ri.Cvar_Get' 'deferred specular strength cvar'
check "$TR_INIT" 'r_deferredAOCoupling = ri.Cvar_Get' 'deferred AO coupling cvar'
check "$TR_INIT" 'r_deferredDefaultMetalness = ri.Cvar_Get' 'deferred fallback metalness cvar'
check "$TR_INIT" 'r_deferredNormalEdgeThreshold = ri.Cvar_Get' 'deferred normal edge threshold cvar'
check "$TR_INIT" 'r_deferredGBufferDebug, "0", "6"' 'deferred debug exposes confidence and motion modes'
check "$TR_INIT" 'r_renderMode 1/2' 'G-buffer cvar documents mode 1/2 sidecar'
check "$DGB" 'materialParams' 'G-buffer push carries material/normal params'
check "$DGB" 'r_renderMode->integer == 1 || r_renderMode->integer == 2' 'G-buffer active in mode 1/2'
check "$DGB" 'mode == 1 || mode == 3' 'deferred lighting active in mode 1 and Unified Clustered mode 3'
check "$DGB" 'normal confidence' 'deferred debug logs normal confidence mode'
check "$DGB" 'vk.motion_vector_view' 'deferred debug can inspect real motion sidecar'
check "$DGB" 'direct material/motion export' 'direct export path preserves material shader output'
check "$DGB" 'vk_end_render_pass' 'G-buffer capture leaves render pass before compute'
check "$DGB" 'vk_resume_current_render_pass' 'G-buffer capture resumes main render pass'
check "$ATTACH" 'material RGBA16F' 'deferred scaffold allocates expanded material export target'
check "$ATTACH" 'deferredGbufferDirectExport' 'deferred scaffold selects direct export when safe'
check "$RPASS" 'colorRefs\[2\].attachment = 3' 'main render pass attaches direct normal target'
check "$RPASS" 'colorRefs\[3\].attachment = 4' 'main render pass attaches direct material target'
check "$FBO" 'deferred_gbuffer_normal_view' 'main framebuffer binds direct normal target'
check "$FBO" 'deferred_gbuffer_material_view' 'main framebuffer binds direct material target'
check "$PIPE" 'gbuf_gen' 'pipeline selects PBR G-buffer export variants'
check "$PIPE" 'attachment_blend_states\[2\].colorWriteMask' 'pipeline enables direct normal writes'
check "$PIPE" 'attachment_blend_states\[3\].colorWriteMask' 'pipeline enables direct material writes'
check "$GEN_FRAG" 'out_deferred_normal' 'PBR shader exports deferred normal'
check "$GEN_FRAG" 'out_deferred_material' 'PBR shader exports deferred material'
check "$ROOT/scripts/compile_shaders.sh" 'USE_DEFERRED_EXPORT' 'shader compiler builds deferred export variants'
check "$GBUF_FILL" 'layout(rgba16f, set = 0, binding = 2)' 'G-buffer material storage uses RGBA16F'
check "$GBUF_FILL" 'sourceConfidence' 'G-buffer material stores source confidence channel'
check "$GBUF_FILL" 'confidence' 'G-buffer normal pass stores reconstruction confidence'
check "$GBUF_DEBUG" 'pc.mode == 5' 'deferred debug shader visualizes normal confidence'
check "$GBUF_DEBUG" 'pc.mode == 6' 'deferred debug shader visualizes motion vectors'
check "$TR_BACKEND" 'vk_deferred_lighting_active' 'classic lit pass skipped when deferred lighting active'
check "$DGB" 'vk_deferred_composite_push_t' 'composite push constants'
check "$DGB" 'deferred_gbuffer_albedo_view' 'composite scene base descriptor'
check "$COMP" 'sceneBaseTex' 'composite scene base sampler'
check "$COMP" 'pc.additive' 'composite additive blend'
check "$LIT" 'material.g' 'deferred lighting reads G-buffer roughness'
check "$LIT" 'material.r' 'deferred lighting reads G-buffer metalness'
check "$LIT" 'shadingConfidence' 'deferred lighting confidence-mixes metalness/roughness'
check "$LIT" 'materialAO' 'deferred lighting consumes material AO'
check "$LIT" 'pc.aoStrength' 'deferred lighting exposes AO coupling push constant'
check "$LIT" 'pc.specularStrength' 'deferred lighting exposes specular strength push constant'
check "$LIT" 'pc.specular' 'deferred lighting specular toggle'
check "$LIT" 'specularAcc' 'deferred specular accumulation'
check "$LIT" 'normalsAreWorld' 'deferred lighting world->view normal transform'
check "$LIT" 'useMaterialClass' 'deferred lighting material-class dispatch'
check "$LIT" 'classMap' 'deferred lighting class map binding'
check "$GEN_FRAG" 'bool deferredAdditiveHandoff' 'shared deferred handoff flag keeps mode 1 and mode 3 opaque logic aligned'
check "$GEN_FRAG" 'keep baked' 'deferred additive handoff keeps lightmap/vertex primary as static-lit base'
check "$GEN_FRAG" '!deferredAdditiveHandoff' 'advanced direct lobes stay disabled during deferred additive handoff'
check "$TR_INIT" 'r_deferredMaterialClassify = ri.Cvar_Get' 'r_deferredMaterialClassify cvar'
check "$DGB" 'deferred_class_stub' 'class stub descriptor fallback'
check "$ROOT/renderers/vulkan/tr_init_diagnostics.inc" 'renderer_deferred_safe + vid_restart' 'compatibility warnings point users at deferred-safe recovery'
check "$TR_BACKEND" 'vk_deferred_lighting_apply_after_geometry' 'backend lighting hook'
check "$ROOT/scripts/compile_shaders.sh" 'deferred_lighting_composite_fs' 'composite shader registered'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All deferred lighting wiring checks passed."
