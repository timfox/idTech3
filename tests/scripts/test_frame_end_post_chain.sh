#!/usr/bin/env bash
# Wiring test: frame-end Vulkan post chain validates scene/pipeline ownership before gamma.
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

FRAME_END="$(idtech3_file renderers/vulkan/vk_frame_end.c src/renderers/vulkan/vk_frame_end.c)"

check "$FRAME_END" 'vk_end_frame_validate_post_process_chain' 'frame-end validation helper exists'
check "$FRAME_END" 'vk_end_frame_try_repair_gamma_chain' 'frame-end path exposes gamma-chain self-heal helper'
check "$FRAME_END" 'vk_end_frame_record_emergency_present' 'frame-end path exposes emergency present fallback'
check "$FRAME_END" 'expected no active render pass before frame-end post chain' 'frame-end validation warns on lingering render passes'
check "$FRAME_END" 'FBO path active but color_image_view is null' 'frame-end validation warns on missing HDR color source'
check "$FRAME_END" 'post-fog source is null' 'frame-end validation warns on missing post-fog source'
check "$FRAME_END" 'luminance source is null' 'frame-end validation warns on missing luminance source'
check "$FRAME_END" 'uiOverlayContentValid' 'gamma composes UI overlay from contentValid (survives FinishBloom/post_bloom)'
check "$FRAME_END" 'prepare_post_leave_ui_overlay' 'prepare_post_process ends UI overlay recording before post AA/TAA/gamma'
check "$FRAME_END" 'uiOverlayContentValid=1 but ui_overlay_image_view is null' 'frame-end validation warns on missing UI overlay image when contentValid'
check "$FRAME_END" 'UI overlay content present but overlay_compose render pass is null' 'frame-end validation warns on missing overlay compose pass'
check "$FRAME_END" 'gamma chain self-heal attempted' 'frame-end path logs gamma-chain self-heal attempts'
check "$FRAME_END" 'emergency present fallback copied' 'frame-end path logs emergency present fallback'
check "$FRAME_END" 'VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL' 'emergency present fallback transitions swapchain image for transfer'
check "$FRAME_END" 'vk_update_attachment_descriptors();' 'frame-end gamma self-heal refreshes attachment descriptors'
check "$FRAME_END" 'vk_update_post_process_pipelines();' 'frame-end gamma self-heal refreshes post-process pipelines'
check "$FRAME_END" 'vk_end_frame_validate_post_process_chain( "prepare_post_process"' 'prepare_post_process validates post chain'
check "$FRAME_END" 'vk_end_frame_validate_post_process_chain( "gamma_pass"' 'gamma pass validates post chain'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All frame-end post-chain wiring checks passed."
