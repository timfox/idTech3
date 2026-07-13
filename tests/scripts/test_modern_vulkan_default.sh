#!/usr/bin/env bash
# Guard the documented modern Vulkan default profile.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

CFG="config/modern_vulkan.cfg"
NATIVE="config/modern_native.cfg"
TR_INIT="renderers/vulkan/tr_init.c"
DGB="renderers/vulkan/vk_deferred_gbuffer.c"
ATTACH="renderers/vulkan/vk_attachments.c"

[ -f "$CFG" ] || fail "missing $CFG"
[ -f "$NATIVE" ] || fail "missing $NATIVE"

grep -q 'exec modern_vulkan.cfg' "$NATIVE" || fail "modern_native must inherit modern_vulkan"
grep -q 'modern_vulkan.cfg' scripts/compile_engine.sh || fail "release packaging must ship modern_vulkan.cfg"
grep -q 'modern_vulkan.cfg' runtime/client/core/cl_cvars.c || fail "auto profile help must mention modern_vulkan"

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

grep -q 'r_renderMode 1/2' "$TR_INIT" || fail "cvar docs must say G-buffer works in render modes 1/2"
grep -q 'r_renderMode->integer == 1 || r_renderMode->integer == 2' "$DGB" || fail "G-buffer active helper must allow mode 2 sidecar"
grep -q 'r_renderMode->integer != 1 && r_renderMode->integer != 2' "$ATTACH" || fail "G-buffer allocation must allow mode 2 sidecar"
grep -q 'r_renderMode->integer == 1 && r_forwardPlus' "$DGB" || fail "deferred lighting must remain mode 1 only"

grep -q 'modern_vulkan.cfg' docs/RENDERERS.md || fail "RENDERERS.md must document the modern Vulkan default"
grep -q 'deferred G-buffer sidecar' docs/RENDERERS.md || fail "RENDERERS.md must document G-buffer sidecar"

pass "modern Vulkan default profile contract"
