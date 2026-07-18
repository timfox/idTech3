#!/usr/bin/env bash
# Wiring test: deferred/visibility/OIT compute/pass breaks validate resume assumptions.
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

DGB="$(idtech3_file renderers/vulkan/vk_deferred_gbuffer.c src/renderers/vulkan/vk_deferred_gbuffer.c)"
VISBUF="$(idtech3_file renderers/vulkan/vk_visibility_buffer.c src/renderers/vulkan/vk_visibility_buffer.c)"
POSTFX="$(idtech3_file renderers/vulkan/vk_postfx_passes.c src/renderers/vulkan/vk_postfx_passes.c)"

check "$DGB" 'vk_dgb_validate_compute_break' 'deferred compute-break validator exists'
check "$DGB" 'expected out-of-pass compute/transfer window' 'deferred validator warns on lingering render pass'
check "$DGB" 'resume_main requested but renderPassIndex=' 'deferred validator warns on bad main-pass resume state'
check "$DGB" 'command buffer unavailable during deferred compute break' 'deferred validator warns on missing command buffer'
check "$DGB" 'vk_dgb_validate_compute_break( "gbuffer_capture_after_geometry"' 'deferred capture invokes compute-break validation'

check "$VISBUF" 'vk_visbuf_validate_compute_break' 'visibility compute-break validator exists'
check "$VISBUF" 'expected out-of-pass compute window' 'visibility validator warns on lingering render pass'
check "$VISBUF" 'command buffer unavailable during visibility compute break' 'visibility validator warns on missing command buffer'
check "$VISBUF" 'vk_visbuf_validate_compute_break( "visibility_buffer_capture_after_geometry"' 'visibility capture invokes compute-break validation'

check "$POSTFX" 'vk_oit_validate_pass_break' 'OIT pass-break validator exists'
check "$POSTFX" 'expected no active render pass before OIT side pass' 'OIT validator warns on lingering render pass'
check "$POSTFX" 'expected transparent drawSurfFilter 2 or 0' 'OIT validator warns on wrong filter state'
check "$POSTFX" 'vk_oit_validate_pass_break( "oit_pass_begin"' 'OIT pass invokes validation after leaving main pass'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All compute-break validation wiring checks passed."
