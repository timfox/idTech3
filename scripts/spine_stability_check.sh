#!/usr/bin/env bash
# Spine stability static contract: mouse input, stable profile, WBOIT guards, OIT deps.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

echo "=== Renderer Spine stability check ==="

# 1. Mouse / input
grep -q 'input_status' engine/platform/sdl/sdl_input.c || fail "input_status missing"
grep -q 'IN_SetRelativeMouse' engine/platform/sdl/sdl_input.c || fail "relative mouse helper missing"
grep -q 'mouse_frac_x' engine/platform/sdl/sdl_input.c || fail "float mouse accum missing"
grep -q 'pixel_width' engine/platform/sdl/sdl_glw.h || fail "pixel_width missing"
pass "mouse-look lifecycle symbols present"

# 2. Stable profile default
[[ -f config/modern_vulkan_stable.cfg ]] || fail "modern_vulkan_stable.cfg missing"
[[ -f config/modern_vulkan_quality.cfg ]] || fail "modern_vulkan_quality.cfg missing"
[[ -f config/modern_vulkan_rt.cfg ]] || fail "modern_vulkan_rt.cfg missing"
[[ -f config/modern_vulkan_experimental.cfg ]] || fail "modern_vulkan_experimental.cfg missing"
[[ -f config/gfx_safe.cfg ]] || fail "gfx_safe.cfg missing"
grep -q 'exec modern_vulkan_stable.cfg' config/modern_vulkan.cfg || fail "modern_vulkan.cfg must exec stable"
grep -q 'seta r_renderMode 2' config/modern_vulkan_stable.cfg || fail "stable must be Forward+ mode 2"
grep -q 'seta r_taa 0' config/modern_vulkan_stable.cfg || fail "stable must disable TAA"
grep -q 'seta r_oit 0' config/modern_vulkan_stable.cfg || fail "stable must disable OIT"
grep -q 'seta r_stochasticAlpha 0' config/modern_vulkan_stable.cfg || fail "stable must disable stochastic alpha"
grep -q 'seta r_hybrid1 0' config/modern_vulkan_stable.cfg || fail "stable must disable Hybrid1"
grep -q 'seta r_visibilityBuffer 0' config/modern_vulkan_stable.cfg || fail "stable must disable vis buffer"
grep -q 'seta r_openWorld 0' config/modern_vulkan_stable.cfg || fail "stable must disable open-world"
grep -q 'seta r_ambientVisibilityMode 2' config/modern_vulkan_stable.cfg || fail "stable must use GTAO"
grep -q 'seta r_ssao 0' config/modern_vulkan_stable.cfg || fail "stable must disable SSAO"
grep -q 'seta r_oit 1' config/modern_vulkan_quality.cfg || fail "quality must enable WBOIT"
pass "stable/quality/rt/experimental/gfx_safe profiles"

# 2b. G-buffer / AV lifecycle contract (does not promote AV mode 4)
bash "$ROOT/scripts/gbuffer_av_lifecycle_check.sh"

# 2c. Restart / temporal ownership (swapchain teardown ↔ restore)
PRES="$ROOT/renderers/vulkan/vk_presentation.c"
TEMP_C="$ROOT/renderers/vulkan/vk_temporal.c"
grep -q 'VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE' "$PRES" || fail "presentation must sticky-reset on swapchain change"
grep -q 'vk_ambient_visibility_reset_history' "$TEMP_C" || fail "temporal apply_resets must reset AV history"
grep -q 'vk_reset_taa_history' "$TEMP_C" || fail "temporal apply_resets must reset TAA history"
grep -q 'appliedResetReasons' "$ROOT/renderers/vulkan/vk_ambient_visibility.c" || \
  fail "AV frame_begin must observe temporal appliedResetReasons"
pass "restart/temporal ownership: swapchain sticky + shared AV/TAA reset"

# 2d. Temporal history ownership contract (weapon / portals / status)
bash "$ROOT/scripts/temporal_ownership_check.sh"

# 3. WBOIT clears + barriers + debug-Z
grep -q 'renderPass == vk.render_pass.oit_accum' renderers/vulkan/vk_render_pass.c || fail "OIT accum clear site missing"
grep -A12 'renderPass == vk.render_pass.oit_accum' renderers/vulkan/vk_render_pass.c | grep -q 'clear_values\[1\].color.float32\[0\] = 1.0f' || fail "OIT reveal clear one missing"
grep -q 'vk_oit_barrier_targets_for_sampling' renderers/vulkan/vk_postfx_passes.c || fail "OIT sample barrier missing"
grep -q 'oit_deps\[0\].dependencyFlags = 0' renderers/vulkan/vk_render_pass.c || fail "OIT must clear BY_REGION deps"
grep -q 'VK_COMPARE_OP_GREATER_OR_EQUAL' renderers/vulkan/vk_pipeline_helpers.c || fail "OIT reversed-Z missing"
grep -q 'texelFetch( oitAccumTex' renderers/vulkan/shaders/glsl/oit_resolve.frag || fail "OIT resolve must texelFetch"
grep -q 'clamp( base.a, 0.0, 0.999 )' renderers/vulkan/shaders/glsl/oit_accum.frag || fail "WBOIT alpha clamp missing"
grep -q 'mode == 12' renderers/vulkan/shaders/glsl/oit_resolve.frag || fail "OIT fragment-count debug missing"
grep -q 'mode == 13' renderers/vulkan/shaders/glsl/oit_resolve.frag || fail "OIT transparent-depth debug missing"
pass "WBOIT clears/barriers/debug/reversed-Z"

# 4. Horizontal corruption guards
grep -q 'GL_NEAREST' renderers/vulkan/vk_descriptor_sets.c || fail "NEAREST samplers missing"
grep -q 'vk_get_active_render_extent' renderers/vulkan/vk_volumetric_internal.c || fail "MSAA depth dispatch extent missing"
pass "horizontal striping guards"

# 5. Packaging
grep -q 'modern_vulkan_stable.cfg' scripts/compile_engine.sh || fail "compile_engine must package stable"
grep -q 'gfx_safe.cfg' scripts/compile_engine.sh || fail "compile_engine must package gfx_safe"
pass "release packaging lists Spine configs"

echo "=== Spine stability check PASSED ==="
echo "Manual GPU matrix still required: menu, maps, WBOIT (quality), weapon, resize,"
echo "fullscreen, alt-tab, vid_restart, clean shutdown, input_status during look."
