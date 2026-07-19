#!/usr/bin/env bash
# Smoke checks for MBOIT + stochastic alpha-clipped materials.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

test -f "$ROOT/docs/MOMENT_OIT_STOCHASTIC_ALPHA.md" || fail "docs missing"
test -f "$ROOT/config/vulkan_overlay_mboit.cfg" || fail "overlay missing"
test -f "$ROOT/config/vulkan_overlay_oit_clustered.cfg" || fail "oit clustered overlay missing"
test -f "$ROOT/examples/demo_game/mod/demo_oit_clustered.cfg" || fail "oit clustered demo missing"
test -f "$ROOT/renderers/vulkan/shaders/glsl/oit_moments.frag" || fail "oit_moments.frag missing"
test -f "$ROOT/renderers/vulkan/shaders/glsl/oit_accum_mboit.frag" || fail "oit_accum_mboit.frag missing"

rg -q 'CheckRange\( r_oit, "0", "2"' "$ROOT/renderers/vulkan/tr_init.c" || fail "r_oit range 0-2"
rg -q 'r_oitForwardPlus' "$ROOT/renderers/vulkan/tr_init.c" || fail "r_oitForwardPlus cvar"
rg -q 'forward_plus_lit' "$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag" || fail "Forward+-lit OIT frag"
rg -q 'forward_plus_lit' "$ROOT/renderers/vulkan/shaders/glsl/oit_accum_mboit.frag" || fail "Forward+-lit MBOIT accum frag"
rg -q 'set = 4' "$ROOT/renderers/vulkan/shaders/glsl/oit_accum_mboit.frag" || fail "MBOIT Forward+ set 4"
rg -q 'frag_world_pos' "$ROOT/renderers/vulkan/shaders/glsl/oit_accum.vert" || fail "OIT world pos VS"
rg -q 'set_layouts\[4\] = vk.set_layout_forward_plus' "$ROOT/renderers/vulkan/vk_init_device.c" || fail "MBOIT accum layout set 4"
rg -q 'pipeline_layout_oit_accum_mboit' "$ROOT/renderers/vulkan/vk_draw_state.c" || fail "MBOIT accum bind path"
rg -q 'vk_forward_plus_get_graphics_descriptor_set' "$ROOT/renderers/vulkan/vk_draw_state.c" || fail "Forward+ bind helper used"
rg -q 'Forward\+-lit OIT accumulation' "$ROOT/renderers/vulkan/tr_init.c" || fail "r_oitForwardPlus covers MBOIT"
rg -q 'r_stochasticAlpha' "$ROOT/renderers/vulkan/tr_init.c" || fail "r_stochasticAlpha cvar"
rg -q 'oit_moments' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || fail "moments pass wiring"
rg -q 'StochasticIGN|stochMode' "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" || fail "stochastic gen_frag"
rg -q 'stochMode' "$ROOT/renderers/vulkan/shaders/glsl/light_frag.tmpl" || fail "stochastic light_frag"
rg -q 'reserved\[6\]' "$ROOT/renderers/vulkan/vk_view_state.c" || fail "stoch push seed"
rg -q 'r_oit 2' "$ROOT/config/vulkan_overlay_oit_clustered.cfg" || fail "clustered overlay sets MBOIT"
rg -q 'vulkan_overlay_unified_clustered' "$ROOT/config/vulkan_overlay_oit_clustered.cfg" || fail "clustered overlay stacks mode 3"

rg -q 'r_oitClassify' "$ROOT/renderers/vulkan/tr_init.c" || fail "r_oitClassify cvar"
rg -q 'oitBucketFilter' "$ROOT/renderers/vulkan/tr_backend.c" || fail "oitBucketFilter draw filter"
rg -q 'r_oitClassify' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || fail "oit classify bucket loop"
rg -q 'r_oitClassify' "$ROOT/docs/MOMENT_OIT_STOCHASTIC_ALPHA.md" || fail "oit classify docs"

echo "OK: MBOIT + stochastic alpha smoke checks passed"
