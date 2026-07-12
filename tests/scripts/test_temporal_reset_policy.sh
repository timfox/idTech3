#!/usr/bin/env bash
# Wiring test: shared temporal reset policy and its core consumers.
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

VK_TEMPORAL_H="$(idtech3_file renderers/vulkan/vk_temporal.h src/renderers/vulkan/vk_temporal.h)"
VK_TEMPORAL_C="$(idtech3_file renderers/vulkan/vk_temporal.c src/renderers/vulkan/vk_temporal.c)"
VK_OCCLUSION="$(idtech3_file renderers/vulkan/vk_occlusion.c src/renderers/vulkan/vk_occlusion.c)"
VK_VIEW_STATE="$(idtech3_file renderers/vulkan/vk_view_state.c src/renderers/vulkan/vk_view_state.c)"
RENDERERS_DOC="$ROOT/docs/RENDERERS.md"

check "$VK_TEMPORAL_H" 'VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE' 'render-size reset reason is declared'
check "$VK_TEMPORAL_H" 'vk_temporal_request_sticky_reset' 'sticky reset API is declared'
check "$VK_TEMPORAL_C" 'vk_reset_occlusion_visibility' 'temporal reset clears occlusion visibility'
check "$VK_TEMPORAL_C" 'vk_get_render_target_width()' 'temporal reset uses effective render-target width'
check "$VK_TEMPORAL_C" 'vk_get_render_target_height()' 'temporal reset uses effective render-target height'
check "$VK_TEMPORAL_C" 'vk_temporal_compute_shared_camera_cut' 'shared camera-cut helper present'
check "$VK_OCCLUSION" 'Stale visibility after camera cut / world change' 'occlusion code documents temporal invalidation contract'
check "$VK_VIEW_STATE" 'vk_get_render_target_width' 'shared render-target helper exists'
check "$RENDERERS_DOC" 'Shared temporal reset policy' 'renderer docs mention shared temporal reset policy'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All temporal reset policy wiring checks passed."
