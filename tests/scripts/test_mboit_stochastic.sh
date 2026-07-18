#!/usr/bin/env bash
# Smoke checks for MBOIT + stochastic alpha-clipped materials.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

test -f "$ROOT/docs/MOMENT_OIT_STOCHASTIC_ALPHA.md" || fail "docs missing"
test -f "$ROOT/config/vulkan_overlay_mboit.cfg" || fail "overlay missing"
test -f "$ROOT/renderers/vulkan/shaders/glsl/oit_moments.frag" || fail "oit_moments.frag missing"
test -f "$ROOT/renderers/vulkan/shaders/glsl/oit_accum_mboit.frag" || fail "oit_accum_mboit.frag missing"

rg -q 'CheckRange\( r_oit, "0", "2"' "$ROOT/renderers/vulkan/tr_init.c" || fail "r_oit range 0-2"
rg -q 'r_stochasticAlpha' "$ROOT/renderers/vulkan/tr_init.c" || fail "r_stochasticAlpha cvar"
rg -q 'oit_moments' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || fail "moments pass wiring"
rg -q 'StochasticIGN|stochMode' "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" || fail "stochastic gen_frag"
rg -q 'stochMode' "$ROOT/renderers/vulkan/shaders/glsl/light_frag.tmpl" || fail "stochastic light_frag"
rg -q 'reserved\[6\]' "$ROOT/renderers/vulkan/vk_view_state.c" || fail "stoch push seed"

echo "OK: MBOIT + stochastic alpha smoke checks passed"
