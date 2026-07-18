#!/usr/bin/env bash
# Wiring test: Unified Clustered transparent handoff validates Forward+ vs OIT ownership.
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

BACKEND="$(idtech3_file renderers/vulkan/tr_backend.c src/renderers/vulkan/tr_backend.c)"

check "$BACKEND" 'RB_ValidateUnifiedClusteredTransparentHandoff' 'backend exposes mode-3 transparent handoff validator'
check "$BACKEND" 'RB_RepairUnifiedClusteredTransparentHandoff' 'backend exposes mode-3 transparent handoff self-heal helper'
check "$BACKEND" 'transparent handoff: deferred lighting inactive before' 'mode-3 handoff warns if deferred opaque stage did not activate'
check "$BACKEND" 'transparent handoff: expected drawSurfFilter=2 before' 'mode-3 handoff warns on wrong transparent filter'
check "$BACKEND" 'OIT handoff: expected no active render pass before vk_oit_pass' 'mode-3 OIT handoff warns on lingering render pass'
check "$BACKEND" 'transparent Forward+ handoff: expected active main render pass' 'mode-3 transparent Forward+ warns when main pass was not restored'
check "$BACKEND" 'transparent handoff self-heal: restoring drawSurfFilter=2 before' 'mode-3 handoff can restore transparent draw filter'
check "$BACKEND" 'OIT handoff self-heal: ending lingering' 'mode-3 OIT handoff can end stale render passes'
check "$BACKEND" 'transparent Forward+ handoff self-heal: resuming main render pass before transparent shade' 'mode-3 Forward+ handoff can resume main render pass'
check "$BACKEND" 'RB_ValidateUnifiedClusteredTransparentHandoff( qtrue )' 'mode-3 OIT path is validated'
check "$BACKEND" 'RB_ValidateUnifiedClusteredTransparentHandoff( qfalse )' 'mode-3 Forward+ transparent path is validated'
check "$BACKEND" 'RB_RepairUnifiedClusteredTransparentHandoff( qtrue )' 'mode-3 OIT path is self-healed'
check "$BACKEND" 'RB_RepairUnifiedClusteredTransparentHandoff( qfalse )' 'mode-3 Forward+ transparent path is self-healed'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All Unified Clustered handoff wiring checks passed."
