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
TR_DIAG="$(idtech3_file renderers/vulkan/diagnostics/tr_init_diagnostics.inc src/renderers/vulkan/diagnostics/tr_init_diagnostics.inc)"
POST_FOG_H="$(idtech3_file renderers/vulkan/vk_post_fog.h src/renderers/vulkan/vk_post_fog.h)"

check "$TR_INIT" '#include "vk_post_fog.h"' 'renderer init includes post-fog diagnostics helpers'
check "$TR_DIAG" 'R_RenderPassName' 'diagnostics expose render-pass naming helper'
check "$TR_DIAG" 'R_RendererSceneSourceHealthy' 'diagnostics expose scene-source health helper'
check "$TR_DIAG" 'R_RendererSwapchainHealthy' 'diagnostics expose swapchain health helper'
check "$TR_DIAG" 'R_RendererSwapchainLastRestartName' 'diagnostics expose last swapchain restart helper'
check "$TR_DIAG" 'R_RendererSwapchainRestartRecent' 'diagnostics expose recent swapchain restart helper'
check "$TR_DIAG" 'R_RendererCountIncompleteCubemaps' 'diagnostics expose cubemap completeness helper'
check "$TR_DIAG" 'runtime   : forward+=%s deferred=%s clustered=%s' 'renderer_health prints runtime readiness summary'
check "$TR_DIAG" 'passes    : inRenderPass=%s active=%s uiOverlay=%s sourcesHealthy=%s' 'renderer_status prints pass ownership summary'
check "$TR_DIAG" 'present   : swapchainHealthy=%s extent=%ux%u images=%u fullscreen=%d srgb=%d' 'renderer_status prints presentation summary'
check "$TR_DIAG" 'restart   : count=%u last=%s' 'renderer_status prints swapchain restart summary'
check "$TR_DIAG" 'restartAt : recent=%s ms=%d' 'renderer_status prints swapchain restart timing summary'
check "$TR_DIAG" 'sources   : postFog=%s scene=%s luminance=%s' 'renderer_status prints scene-source summary'
check "$TR_DIAG" 'cubemaps  : runtime=%s total=%d ready=%d incomplete=%d hdrFallback=%s localReady=%s debug=' 'renderer_status prints cubemap readiness summary'
check "$TR_DIAG" 'mode want : fp=%s gbuffer=%s deferred=%s split=%s pt=%s default=%s' 'renderer_status prints render mode feature contract'
check "$TR_DIAG" 'gbuffer   : req=%s alloc=%s fill=%s gen=%u valid=%s extent=%ux%u view=%s' 'renderer_status prints deferred runtime readiness summary'
check "$TR_DIAG" 'passes: inRenderPass=%s active=%s uiOverlay=%s sourcesHealthy=%s' 'renderer_compatibility prints pass ownership summary'
check "$TR_DIAG" 'present: swapchainHealthy=%s extent=%ux%u images=%u fullscreen=%d srgb=%d' 'renderer_compatibility prints presentation summary'
check "$TR_DIAG" 'restart: count=%u last=%s' 'renderer_compatibility prints swapchain restart summary'
check "$TR_DIAG" 'restartAt: recent=%s ms=%d' 'renderer_compatibility prints swapchain restart timing summary'
check "$TR_DIAG" 'sources: postFog=%s scene=%s luminance=%s' 'renderer_compatibility prints scene-source summary'
check "$TR_DIAG" 'cubemaps: runtime=%s total=%d ready=%d incomplete=%d hdrFallback=%s localReady=%s debug=' 'renderer_compatibility prints cubemap readiness summary'
check "$TR_DIAG" 'passes     : inRenderPass=%s active=%s uiOverlay=%s sourcesHealthy=%s' 'renderer_subsystems prints pass ownership summary'
check "$TR_DIAG" 'present    : swapchainHealthy=%s extent=%ux%u images=%u fullscreen=%d srgb=%d' 'renderer_subsystems prints presentation summary'
check "$TR_DIAG" 'restart    : count=%u last=%s' 'renderer_subsystems prints swapchain restart summary'
check "$TR_DIAG" 'restartAt  : recent=%s ms=%d' 'renderer_subsystems prints swapchain restart timing summary'
check "$TR_DIAG" 'sources    : postFog=%s scene=%s luminance=%s' 'renderer_subsystems prints scene-source summary'
check "$TR_DIAG" 'cubemaps   : runtime=%s total=%d ready=%d incomplete=%d hdrFallback=%s localReady=%s debug=' 'renderer_subsystems prints cubemap readiness summary'
check "$TR_DIAG" 'runtime    : forward+=%s deferred=%s clustered=%s' 'renderer_subsystems prints runtime readiness summary'
check "$TR_DIAG" 'RENDER_PASS_UI_OVERLAY' 'scene-source health considers UI overlay render-pass ownership'
check "$POST_FOG_H" 'vk_get_luminance_source' 'post-fog API exposes luminance source getter'
check "$POST_FOG_H" 'vk_post_fog_source_name' 'post-fog API exposes source naming helper'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All renderer pass/source diagnostics wiring checks passed."
