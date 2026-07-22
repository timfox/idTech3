#!/usr/bin/env bash
# Static gate: mode 3 + production WBOIT (r_oit 1) clustered handoff.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

grep -Eq 'seta[[:space:]]+r_oit[[:space:]]+1' "$ROOT/config/vulkan_overlay_oit_clustered.cfg" || \
	fail 'vulkan_overlay_oit_clustered.cfg must set r_oit 1'
grep -Eq 'seta[[:space:]]+r_oit[[:space:]]+1' "$ROOT/config/modern_clustered.cfg" || \
	fail 'modern_clustered.cfg must set r_oit 1'
grep -q 'vk_cluster_assert_shared_consumers' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || \
	fail 'vk_oit_pass must call vk_cluster_assert_shared_consumers'
grep -iq 'WBOIT' "$ROOT/docs/UNIFIED_CLUSTERED_RENDERER.md" || \
	fail 'UNIFIED_CLUSTERED_RENDERER.md must mention WBOIT'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT mode-3 wiring checks passed."
exit 0
