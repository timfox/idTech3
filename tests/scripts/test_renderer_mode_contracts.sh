#!/usr/bin/env bash
# Wiring test: renderer diagnostics should expose mode-contract cleanliness and recovery commands.
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

TR_DIAG="$(idtech3_file renderers/vulkan/tr_init_diagnostics.inc src/renderers/vulkan/tr_init_diagnostics.inc)"
TR_INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"

check "$TR_DIAG" 'R_RendererRecommendedRecoveryCommand' 'diagnostics expose renderer recovery helper'
check "$TR_DIAG" 'R_RendererModeContractClean' 'diagnostics expose mode-contract cleanliness helper'
check "$TR_DIAG" 'contract  : clean=%s recovery=%s' 'renderer_status prints contract cleanliness and recovery'
check "$TR_DIAG" 'contract: clean=%s recovery=%s' 'renderer_compatibility prints contract cleanliness and recovery'
check "$TR_DIAG" 'contract   : clean=%s recovery=%s' 'renderer_subsystems prints contract cleanliness and recovery'
check "$TR_DIAG" 'renderer_modern_safe' 'modern recovery command participates in contract reporting'
check "$TR_DIAG" 'renderer_deferred_safe' 'deferred recovery command participates in contract reporting'
check "$TR_DIAG" 'renderer_clustered_safe' 'clustered recovery command participates in contract reporting'
check "$TR_DIAG" 'mode-1 deferred expects r_forwardPlusShade 0' 'mode-1 compatibility warns on Forward+ shade drift'
check "$TR_DIAG" 'mode-1 deferred expects r_deferredGBuffer 1 and r_deferredGBufferFill 1' 'mode-1 compatibility warns on missing G-buffer scaffold'
check "$TR_INIT" 'ri.Cmd_AddCommand( "renderer_status"' 'renderer status command still registered'
check "$TR_INIT" 'ri.Cmd_AddCommand( "renderer_compatibility"' 'renderer compatibility command still registered'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All renderer mode contract wiring checks passed."
