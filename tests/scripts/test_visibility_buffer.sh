#!/usr/bin/env bash
# Smoke checks for 2027 visibility-buffer foundation (Phase 1).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

test -f "$ROOT/docs/RENDERER_2027.md" || fail "docs/RENDERER_2027.md missing"
test -f "$ROOT/config/vulkan_overlay_visibility_2027.cfg" || fail "overlay cfg missing"
test -f "$ROOT/examples/demo_game/mod/demo_visibility_2027.cfg" || fail "demo cfg missing"
test -f "$ROOT/renderers/vulkan/vk_visibility_buffer.c" || fail "vk_visibility_buffer.c missing"
test -f "$ROOT/renderers/vulkan/vk_visibility_buffer.h" || fail "vk_visibility_buffer.h missing"
test -f "$ROOT/renderers/vulkan/shaders/glsl/visibility_buffer_fill.comp" || fail "fill shader missing"
test -f "$ROOT/renderers/vulkan/shaders/glsl/material_classify.comp" || fail "classify shader missing"
test -f "$ROOT/renderers/vulkan/shaders/glsl/visibility_buffer_debug.frag" || fail "debug shader missing"

rg -q 'r_visibilityBuffer = ri.Cvar_Get' "$ROOT/renderers/vulkan/tr_init.c" || fail "r_visibilityBuffer cvar missing"
rg -q 'r_visibilityBufferFill = ri.Cvar_Get' "$ROOT/renderers/vulkan/tr_init.c" || fail "r_visibilityBufferFill cvar missing"
rg -q 'r_materialClassify = ri.Cvar_Get' "$ROOT/renderers/vulkan/tr_init.c" || fail "r_materialClassify cvar missing"
rg -q 'r_deferredMaterialClassify = ri.Cvar_Get' "$ROOT/renderers/vulkan/tr_init.c" || fail "r_deferredMaterialClassify cvar missing"
rg -q 'r_deferredMaterialClassify", "0"' "$ROOT/renderers/vulkan/tr_init.c" || fail "r_deferredMaterialClassify should default to 0"
rg -q 'CLASS_ALPHA_TEST' "$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting.comp" || fail "deferred ALPHA_TEST handling missing"
rg -Fq 'matClass = CLASS_SIMPLE_OPAQUE' "$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting.comp" || fail "ALPHA_TEST should remap to opaque, not black-out"
rg -q 'confidence' "$ROOT/renderers/vulkan/shaders/glsl/deferred_gbuffer_fill.comp" || fail "material confidence write missing"
rg -q 'useMaterialClass' "$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting.comp" || fail "deferred lighting class dispatch missing"
rg -q 'normalsAreWorld' "$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting.comp" || fail "deferred normal-space fix missing"
rg -q 'vk_visibility_buffer_capture_after_geometry' "$ROOT/renderers/vulkan/tr_backend.c" || fail "backend capture missing"
rg -q 'vk_create_visibility_buffer_scaffold' "$ROOT/renderers/vulkan/vk_attachments.c" || fail "scaffold alloc missing"
rg -q 'visibilityBufferAllocated' "$ROOT/renderers/vulkan/vk.h" || fail "vk.h fields missing"
rg -q 'visibility_buffer_fill_cs' "$ROOT/scripts/compile_shaders.sh" || fail "compile_shaders entry missing"
rg -q 'r_visibilityBuffer 1' "$ROOT/config/vulkan_overlay_visibility_2027.cfg" || fail "overlay sets vis buffer"
rg -q 'RENDERER_2027' "$ROOT/docs/RENDERERS.md" || fail "RENDERERS.md cross-link missing"
rg -q 'visibility_buffer_status' "$ROOT/renderers/vulkan/tr_init.c" || fail "status command missing"
rg -q 'CheckRange\( r_visibilityBufferDebug, "0", "5"' "$ROOT/renderers/vulkan/tr_init.c" || fail "debug mode range should be 0-5"
rg -q 'morton2d5' "$ROOT/renderers/vulkan/shaders/glsl/visibility_buffer_fill.comp" || fail "P1.5 Morton fill missing"
rg -q 'pc.mode == 5' "$ROOT/renderers/vulkan/shaders/glsl/visibility_buffer_debug.frag" || fail "late-shade debug mode 5 missing"
rg -q 'albedoTex' "$ROOT/renderers/vulkan/shaders/glsl/visibility_buffer_debug.frag" || fail "albedo binding missing"
rg -q 'deferred_gbuffer_albedo_view' "$ROOT/renderers/vulkan/vk_visibility_buffer.c" || fail "albedo descriptor wiring missing"

echo "OK: visibility buffer Phase 1 / 1.5 smoke checks passed"
