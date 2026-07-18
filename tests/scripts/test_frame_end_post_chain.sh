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
check "$FRAME_END" 'expected no active render pass before frame-end post chain' 'frame-end validation warns on lingering render passes'
check "$FRAME_END" 'FBO path active but color_image_view is null' 'frame-end validation warns on missing HDR color source'
check "$FRAME_END" 'post-fog source is null' 'frame-end validation warns on missing post-fog source'
check "$FRAME_END" 'luminance source is null' 'frame-end validation warns on missing luminance source'
check "$FRAME_END" 'uiOverlayActive=1 but ui_overlay_image_view is null' 'frame-end validation warns on missing UI overlay image'
check "$FRAME_END" 'uiOverlayActive=1 but overlay_compose render pass is null' 'frame-end validation warns on missing overlay compose pass'
check "$FRAME_END" 'vk_end_frame_validate_post_process_chain( "prepare_post_process"' 'prepare_post_process validates post chain'
check "$FRAME_END" 'vk_end_frame_validate_post_process_chain( "gamma_pass"' 'gamma pass validates post chain'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All frame-end post-chain wiring checks passed."
