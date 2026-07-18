#!/usr/bin/env bash
# Wiring test: renderer diagnostics should expose render-pass and scene-source ownership.
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

TR_INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"
TR_DIAG="$(idtech3_file renderers/vulkan/tr_init_diagnostics.inc src/renderers/vulkan/tr_init_diagnostics.inc)"
POST_FOG_H="$(idtech3_file renderers/vulkan/vk_post_fog.h src/renderers/vulkan/vk_post_fog.h)"

check "$TR_INIT" '#include "vk_post_fog.h"' 'renderer init includes post-fog diagnostics helpers'
check "$TR_DIAG" 'R_RenderPassName' 'diagnostics expose render-pass naming helper'
check "$TR_DIAG" 'R_RendererSceneSourceHealthy' 'diagnostics expose scene-source health helper'
check "$TR_DIAG" 'passes    : inRenderPass=%s active=%s uiOverlay=%s sourcesHealthy=%s' 'renderer_status prints pass ownership summary'
check "$TR_DIAG" 'sources   : postFog=%s scene=%s luminance=%s' 'renderer_status prints scene-source summary'
check "$TR_DIAG" 'passes: inRenderPass=%s active=%s uiOverlay=%s sourcesHealthy=%s' 'renderer_compatibility prints pass ownership summary'
check "$TR_DIAG" 'sources: postFog=%s scene=%s luminance=%s' 'renderer_compatibility prints scene-source summary'
check "$TR_DIAG" 'passes     : inRenderPass=%s active=%s uiOverlay=%s sourcesHealthy=%s' 'renderer_subsystems prints pass ownership summary'
check "$TR_DIAG" 'sources    : postFog=%s scene=%s luminance=%s' 'renderer_subsystems prints scene-source summary'
check "$TR_DIAG" 'RENDER_PASS_UI_OVERLAY' 'scene-source health considers UI overlay render-pass ownership'
check "$POST_FOG_H" 'vk_get_luminance_source' 'post-fog API exposes luminance source getter'
check "$POST_FOG_H" 'vk_post_fog_source_name' 'post-fog API exposes source naming helper'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All renderer pass/source diagnostics wiring checks passed."
