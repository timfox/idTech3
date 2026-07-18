#!/usr/bin/env bash
# Wiring test: scene-pass helpers validate begin/resume assumptions for Vulkan render passes.
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

SCENE_PASS="$(idtech3_file renderers/vulkan/vk_scene_pass.c src/renderers/vulkan/vk_scene_pass.c)"

check "$SCENE_PASS" 'vk_scene_pass_validate_begin' 'scene-pass begin validation helper exists'
check "$SCENE_PASS" 'vk_scene_pass_validate_resume' 'scene-pass resume validation helper exists'
check "$SCENE_PASS" 'expected no active render pass before entering' 'scene-pass begin validation warns on nested pass begin'
check "$SCENE_PASS" 'render pass handle is null for' 'scene-pass begin validation warns on null render pass handle'
check "$SCENE_PASS" 'framebuffer is null for' 'scene-pass begin validation warns on null framebuffer'
check "$SCENE_PASS" 'resume_current_render_pass: expected out-of-pass state' 'scene-pass resume validation warns on bad resume state'
check "$SCENE_PASS" 'ui overlay resume requested but uiOverlayActive=0' 'scene-pass resume validation warns on UI overlay drift'
check "$SCENE_PASS" 'vk_scene_pass_validate_begin( "begin_main_render_pass"' 'main pass entry is validated'
check "$SCENE_PASS" 'vk_scene_pass_validate_begin( "begin_post_bloom_render_pass"' 'post-bloom pass entry is validated'
check "$SCENE_PASS" 'vk_scene_pass_validate_begin( "begin_ui_overlay_render_pass"' 'UI overlay pass entry is validated'
check "$SCENE_PASS" 'vk_scene_pass_validate_resume();' 'render-pass resume is validated'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All scene-pass validation wiring checks passed."
