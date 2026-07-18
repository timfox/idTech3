#!/usr/bin/env bash
# Smoke checks for Unified Clustered Renderer (r_renderMode 3).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

test -f "$ROOT/docs/UNIFIED_CLUSTERED_RENDERER.md" || fail "docs/UNIFIED_CLUSTERED_RENDERER.md missing"
test -f "$ROOT/config/vulkan_overlay_unified_clustered.cfg" || fail "overlay cfg missing"
test -f "$ROOT/examples/demo_game/mod/demo_unified_clustered.cfg" || fail "demo cfg missing"

rg -q 'case 3:' "$ROOT/renderers/vulkan/tr_render_mode_vk.c" || fail "mode 3 latch missing"
rg -q 'Unified Clustered Renderer' "$ROOT/renderers/vulkan/tr_render_mode_vk.c" || fail "branding log missing"
rg -q 'vk_unified_clustered_active' "$ROOT/renderers/vulkan/vk_deferred_gbuffer.c" || fail "helper missing"
rg -q 'vk_unified_clustered_active' "$ROOT/renderers/vulkan/tr_backend.c" || fail "frame split missing"
rg -q 'drawSurfFilter = 1' "$ROOT/renderers/vulkan/tr_backend.c" || fail "opaque filter missing"
rg -q 'vk_unified_clustered_opaque_handoff' "$ROOT/renderers/vulkan/tr_shade.c" || fail "shade handoff missing"
rg -q 'pbrDebugMode.y' "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" || fail "gen_frag hybrid gate missing"
rg -q 'CheckRange\( r_renderMode, "0", "3"' "$ROOT/renderers/vulkan/tr_init.c" || fail "CheckRange 0-3 missing"
rg -q 'r_renderMode 3' "$ROOT/config/vulkan_overlay_unified_clustered.cfg" || fail "overlay sets mode 3"

echo "OK: unified clustered smoke checks passed"
