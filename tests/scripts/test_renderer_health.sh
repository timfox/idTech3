#!/usr/bin/env bash
# Wiring test: compact renderer health command summarizes Vulkan sanity and recovery.
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

check "$TR_DIAG" 'R_RendererHealthLabel' 'diagnostics expose renderer health label helper'
check "$TR_DIAG" 'R_RendererHealth_f' 'renderer health command implementation exists'
check "$TR_DIAG" '======== Renderer Health ========' 'renderer health banner exists'
check "$TR_DIAG" 'health    : %s' 'renderer health prints top-level status'
check "$TR_DIAG" 'R_RendererUsingRecoveryFallbacks' 'renderer health exposes recovery-fallback helper'
check "$TR_DIAG" 'R_RendererSwapchainRestartRecent' 'renderer health exposes recent swapchain restart helper'
check "$TR_DIAG" 'recovery  : activeFallbacks=%s recentSwapchain=%s' 'renderer health prints active fallback state'
check "$TR_DIAG" 'next      : %s + vid_restart' 'renderer health prints recovery action when unhealthy'
check "$TR_INIT" 'ri.Cmd_AddCommand( "renderer_health"' 'renderer health command is registered'
check "$TR_INIT" 'ri.Cmd_RemoveCommand( "renderer_health"' 'renderer health command is unregistered on shutdown'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All renderer health wiring checks passed."
