#!/usr/bin/env bash
# Wiring test: renderer self-heals obvious resume/source drift in safe Vulkan paths.
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
POST_FOG="$(idtech3_file renderers/vulkan/vk_post_fog.c src/renderers/vulkan/vk_post_fog.c)"
PRESENTATION="$(idtech3_file renderers/vulkan/vk_presentation.c src/renderers/vulkan/vk_presentation.c)"
FRAME_SUBMIT="$(idtech3_file renderers/vulkan/vk_frame_submit.c src/renderers/vulkan/vk_frame_submit.c)"

check "$SCENE_PASS" 'vk_scene_pass_resume_framebuffer' 'scene-pass resume uses a shared framebuffer/self-heal helper'
check "$SCENE_PASS" 'auto-restoring uiOverlayActive for UI overlay resume' 'scene-pass resume repairs UI overlay active flag drift'
check "$SCENE_PASS" 'framebuffer missing for %s, falling back to main' 'scene-pass resume falls back to main pass when a continuation framebuffer is missing'
check "$POST_FOG" 'vk_choose_post_fog_fallback_source' 'post-fog path exposes centralized source fallback helper'
check "$POST_FOG" 'post-fog source fallback' 'post-fog path logs automatic fallback selection'
check "$POST_FOG" 'vk_choose_post_fog_fallback_source( "update_post_fog_descriptors"' 'descriptor updates self-heal null post-fog source'
check "$POST_FOG" 'vk_choose_post_fog_fallback_source( "get_post_fog_source"' 'post-fog getter self-heals null source'
check "$POST_FOG" 'vk_choose_post_fog_fallback_source( "get_luminance_source"' 'luminance getter self-heals null source'
check "$PRESENTATION" 'vk_reset_scene_src_rect_tracking();' 'swapchain restore clears stale scene source rectangle tracking'
check "$PRESENTATION" 'vk_reset_post_fog_frame_state();' 'swapchain restore rebinds post-fog state after fullscreen/resize'
check "$PRESENTATION" 'vk_reset_presentation_runtime_state' 'swapchain restart exposes a shared presentation runtime reset helper'
check "$PRESENTATION" 'swapchain_image_acquired = qfalse;' 'swapchain restart clears per-command image-acquired state'
check "$PRESENTATION" 'swapchain_image_index = 0;' 'swapchain restart clears per-command swapchain image indices'
check "$FRAME_SUBMIT" 'vk.cmd->swapchain_image_index >= vk.swapchain_image_count' 'frame begin validates acquired swapchain image index range'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All renderer self-heal wiring checks passed."
