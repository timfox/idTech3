#!/usr/bin/env bash
# Static gate: OIT cluster generation handoff on mode 3 WBOIT path.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

grep -q 'oitClusterGenAtAccum' "$ROOT/renderers/vulkan/vk.h" || fail 'oitClusterGenAtAccum missing from vk.h'
grep -q 'r_oitClusterDebug' "$ROOT/renderers/vulkan/tr_init.c" || fail 'r_oitClusterDebug not registered'
grep -Eq 'seta[[:space:]]+r_oit[[:space:]]+1' "$ROOT/config/vulkan_overlay_oit_clustered.cfg" || \
	fail 'mode3 OIT overlay must use WBOIT (r_oit 1)'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT cluster handoff wiring checks passed."
exit 0
