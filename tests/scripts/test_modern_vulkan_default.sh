#!/usr/bin/env bash
# Guard the documented modern Vulkan default profile.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

CFG="config/modern_vulkan.cfg"
DEFERRED_OVERLAY="config/vulkan_overlay_deferred.cfg"
RTX_OVERLAY="config/vulkan_overlay_rtx.cfg"
HYBRID_OVERLAY="config/vulkan_overlay_hybrid1.cfg"
NATIVE="config/modern_native.cfg"
TR_INIT="renderers/vulkan/tr_init.c"
DGB="renderers/vulkan/vk_deferred_gbuffer.c"
ATTACH="renderers/vulkan/vk_attachments.c"

[ -f "$CFG" ] || fail "missing $CFG"
[ -f "$DEFERRED_OVERLAY" ] || fail "missing $DEFERRED_OVERLAY"
[ -f "$RTX_OVERLAY" ] || fail "missing $RTX_OVERLAY"
[ -f "$HYBRID_OVERLAY" ] || fail "missing $HYBRID_OVERLAY"
[ -f "$NATIVE" ] || fail "missing $NATIVE"

grep -q 'exec modern_vulkan.cfg' "$NATIVE" || fail "modern_native must inherit modern_vulkan"
grep -q 'modern_vulkan.cfg' scripts/compile_engine.sh || fail "release packaging must ship modern_vulkan.cfg"
grep -q 'modern_vulkan.cfg' runtime/client/core/cl_cvars.c || fail "auto profile help must mention modern_vulkan"
grep -q 'vulkan_overlay_deferred.cfg' scripts/compile_engine.sh || fail "release packaging must ship deferred overlay"
grep -q 'vulkan_overlay_rtx.cfg' scripts/compile_engine.sh || fail "release packaging must ship RTX overlay"
grep -q 'vulkan_overlay_hybrid1.cfg' scripts/compile_engine.sh || fail "release packaging must ship Hybrid1 overlay"
grep -q 'vulkan_overlay_hybrid1.cfg' runtime/client/core/cl_cvars.c || fail "auto profile help must mention Vulkan overlays"

grep -q 'seta r_fbo 1' "$CFG" || fail "modern Vulkan must use FBO"
grep -q 'seta r_hdr 2' "$CFG" || fail "modern Vulkan must use HDR32 default"
grep -q 'seta r_pbr 1' "$CFG" || fail "modern Vulkan must enable PBR"
grep -q 'seta r_materialBlend 1' "$CFG" || fail "modern Vulkan must enable material blending"
grep -q 'seta r_renderMode 2' "$CFG" || fail "modern Vulkan must use Forward+ primary"
grep -q 'seta r_forwardPlus 1' "$CFG" || fail "modern Vulkan must enable Forward+"
grep -q 'seta r_forwardPlusShade 1' "$CFG" || fail "modern Vulkan must enable Forward+ shading"
grep -q 'seta r_forwardPlusDepthCull 1' "$CFG" || fail "modern Vulkan must depth-cull Forward+ tiles"
grep -q 'seta r_deferredGBuffer 1' "$CFG" || fail "modern Vulkan must allocate G-buffer sidecar"
grep -q 'seta r_deferredGBufferFill 1' "$CFG" || fail "modern Vulkan must fill G-buffer sidecar"
grep -q 'seta r_deferredLighting 0' "$CFG" || fail "modern Vulkan default must not enable experimental deferred lighting"
grep -q 'seta r_taa 1' "$CFG" || fail "modern Vulkan must enable TAA"
grep -q 'seta r_taaMotionVectors 1' "$CFG" || fail "modern Vulkan must enable motion-vector TAA"
grep -q 'seta r_tonemap 3' "$CFG" || fail "modern Vulkan must pin Filmic tonemap"
grep -q 'seta r_post 1' "$CFG" || fail "modern Vulkan must keep the post resolve active"
grep -q 'seta r_grade_vibrance 0' "$CFG" || fail "modern Vulkan must default to neutral vibrance"
grep -q 'seta r_grade_hue 0' "$CFG" || fail "modern Vulkan must default to neutral hue"
grep -q 'seta r_post_contrast 1' "$CFG" || fail "modern Vulkan must default to neutral legacy post contrast"
grep -q 'seta r_post_saturation 1' "$CFG" || fail "modern Vulkan must default to neutral legacy post saturation"

grep -q 'exec modern_vulkan.cfg' "$DEFERRED_OVERLAY" || fail "deferred overlay must inherit modern Vulkan"
grep -q 'seta r_renderMode 1' "$DEFERRED_OVERLAY" || fail "deferred overlay must switch to mode 1"
grep -q 'seta r_deferredLighting 1' "$DEFERRED_OVERLAY" || fail "deferred overlay must enable deferred lighting"
grep -q 'seta r_forwardPlusShade 0' "$DEFERRED_OVERLAY" || fail "deferred overlay must prevent double dynamic lighting"
grep -q 'seta r_deferredAOCoupling 0.65' "$DEFERRED_OVERLAY" || fail "deferred overlay must enable AO-coupled dynamic lighting"
grep -q 'seta r_deferredSpecularStrength 1' "$DEFERRED_OVERLAY" || fail "deferred overlay must pin deferred specular strength"

grep -q 'exec modern_vulkan.cfg' "$RTX_OVERLAY" || fail "RTX overlay must inherit modern Vulkan"
grep -q 'seta r_rtx 1' "$RTX_OVERLAY" || fail "RTX overlay must enable r_rtx"
grep -q 'seta r_rtxDemo 1' "$RTX_OVERLAY" || fail "RTX overlay must enable shared TLAS/demo path"
grep -q 'seta r_rtxEntities 1' "$RTX_OVERLAY" || fail "RTX overlay must opt into entity BLAS"

grep -q 'exec modern_vulkan.cfg' "$HYBRID_OVERLAY" || fail "Hybrid1 overlay must inherit modern Vulkan"
grep -q 'seta r_hybrid1 1' "$HYBRID_OVERLAY" || fail "Hybrid1 overlay must enable Hybrid1"
grep -q 'seta r_rtxDemo 1' "$HYBRID_OVERLAY" || fail "Hybrid1 overlay must enable shared TLAS"
grep -q 'seta r_renderMode 1' "$HYBRID_OVERLAY" && fail "Hybrid1 overlay must not replace the modern Forward+ base"

grep -q 'r_renderMode 1/2' "$TR_INIT" || fail "cvar docs must say G-buffer works in render modes 1/2"
grep -q 'r_renderMode->integer == 1 || r_renderMode->integer == 2' "$DGB" || fail "G-buffer active helper must allow mode 2 sidecar"
grep -q 'r_renderMode->integer != 1 && r_renderMode->integer != 2' "$ATTACH" || fail "G-buffer allocation must allow mode 2 sidecar"
grep -q 'r_renderMode->integer == 1 && r_forwardPlus' "$DGB" || fail "deferred lighting must remain mode 1 only"

grep -q 'modern_vulkan.cfg' docs/RENDERERS.md || fail "RENDERERS.md must document the modern Vulkan default"
grep -q 'deferred G-buffer sidecar' docs/RENDERERS.md || fail "RENDERERS.md must document G-buffer sidecar"
grep -q 'vulkan_overlay_hybrid1.cfg' docs/RENDERERS.md || fail "RENDERERS.md must document Vulkan overlays"

pass "modern Vulkan default profile contract"
