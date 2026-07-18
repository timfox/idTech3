#!/usr/bin/env bash
# Smoke checks for Unified Clustered Renderer (r_renderMode 3).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

test -f "$ROOT/docs/UNIFIED_CLUSTERED_RENDERER.md" || fail "docs/UNIFIED_CLUSTERED_RENDERER.md missing"
test -f "$ROOT/config/vulkan_overlay_unified_clustered.cfg" || fail "overlay cfg missing"
test -f "$ROOT/config/vulkan_overlay_unified_clustered_safe.cfg" || fail "safe overlay cfg missing"
test -f "$ROOT/examples/demo_game/mod/demo_unified_clustered.cfg" || fail "demo cfg missing"
test -f "$ROOT/examples/demo_game/mod/demo_unified_clustered_safe.cfg" || fail "safe demo cfg missing"

rg -q 'case 3:' "$ROOT/renderers/vulkan/tr_render_mode_vk.c" || fail "mode 3 latch missing"
rg -q 'Unified Clustered Renderer' "$ROOT/renderers/vulkan/tr_render_mode_vk.c" || fail "branding log missing"
rg -q 'vk_unified_clustered_active' "$ROOT/renderers/vulkan/vk_deferred_gbuffer.c" || fail "helper missing"
rg -q 'vk_unified_clustered_active' "$ROOT/renderers/vulkan/tr_backend.c" || fail "frame split missing"
rg -q 'drawSurfFilter = 1' "$ROOT/renderers/vulkan/tr_backend.c" || fail "opaque filter missing"
rg -q 'vk_unified_clustered_opaque_handoff' "$ROOT/renderers/vulkan/tr_shade.c" || fail "shade handoff missing"
rg -q 'void vk_resume_main_render_pass' "$ROOT/renderers/vulkan/vk_scene_pass.c" || fail "main render-pass resume helper missing"
rg -Fq 'vk_resume_main_render_pass();' "$ROOT/renderers/vulkan/vk_deferred_gbuffer.c" || fail "deferred composite restores main pass before transparent resume"
rg -q 'pbrDebugMode.y' "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" || fail "gen_frag hybrid gate missing"
rg -q 'CheckRange\( r_renderMode, "0", "3"' "$ROOT/renderers/vulkan/tr_init.c" || fail "CheckRange 0-3 missing"
rg -q 'r_renderMode 3' "$ROOT/config/vulkan_overlay_unified_clustered.cfg" || fail "overlay sets mode 3"
rg -q 'renderer_clustered_safe' "$ROOT/renderers/vulkan/tr_init.c" || fail "clustered safe command registered"
rg -q 'exec vulkan_overlay_unified_clustered.cfg' "$ROOT/config/vulkan_overlay_unified_clustered_safe.cfg" || fail "safe overlay layers on mode 3 overlay"
rg -q 'seta r_taa 0' "$ROOT/config/vulkan_overlay_unified_clustered_safe.cfg" || fail "safe overlay disables TAA"
rg -q 'seta r_oit 0' "$ROOT/config/vulkan_overlay_unified_clustered_safe.cfg" || fail "safe overlay disables OIT"
rg -q 'Unified Clustered is safer with TAA/SMAA/FXAA/post-AA disabled while debugging; use renderer_clustered_safe' "$ROOT/renderers/vulkan/tr_init_diagnostics.inc" || fail "compatibility warnings point users at clustered-safe recovery"
rg -q 'skips Forward\+ transparent' "$ROOT/renderers/vulkan/tr_backend.c" || fail "OIT mode3 honesty log missing"
rg -q 'r_oitForwardPlus' "$ROOT/renderers/vulkan/tr_backend.c" || fail "OIT Forward+ lit mode3 log"
rg -q 'vulkan_overlay_oit_clustered' "$ROOT/docs/UNIFIED_CLUSTERED_RENDERER.md" || fail "OIT clustered docs missing"
rg -q 'r_oitForwardPlus' "$ROOT/docs/UNIFIED_CLUSTERED_RENDERER.md" || fail "Forward+-lit OIT docs"
rg -q 'renderer_clustered_safe' "$ROOT/docs/UNIFIED_CLUSTERED_RENDERER.md" || fail "clustered safe command docs missing"
rg -q 'RB_ValidateUnifiedClusteredTransparentHandoff' "$ROOT/renderers/vulkan/tr_backend.c" || fail "mode3 transparent handoff validator missing"
rg -q 'RB_RepairUnifiedClusteredTransparentHandoff' "$ROOT/renderers/vulkan/tr_backend.c" || fail "mode3 transparent handoff self-heal helper missing"
rg -Fq 'transparent Forward+ handoff: expected active main render pass' "$ROOT/renderers/vulkan/tr_backend.c" || fail "mode3 transparent handoff warning missing"
rg -Fq 'transparent Forward+ handoff self-heal: resuming main render pass before transparent shade' "$ROOT/renderers/vulkan/tr_backend.c" || fail "mode3 transparent handoff self-heal log missing"

echo "OK: unified clustered smoke checks passed"
