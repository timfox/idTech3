#!/usr/bin/env bash
# Static gate: MBOIT stays experimental; shipping clustered path uses WBOIT.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

grep -Eq 'seta[[:space:]]+r_oit[[:space:]]+2' "$ROOT/config/modern_vulkan_experimental.cfg" || \
	fail 'modern_vulkan_experimental.cfg must set r_oit 2'
grep -Eq 'seta[[:space:]]+r_oit[[:space:]]+2' "$ROOT/config/vulkan_overlay_mboit.cfg" || \
	fail 'vulkan_overlay_mboit.cfg must set r_oit 2'
grep -q 'MBOIT is experimental and not Spine 1.1 certified' "$ROOT/renderers/vulkan/tr_init.c" || \
	fail 'tr_init.c must warn MBOIT is experimental'
grep -Eq 'seta[[:space:]]+r_oit[[:space:]]+1' "$ROOT/config/vulkan_overlay_oit_clustered.cfg" || \
	fail 'shipping oit_clustered overlay must use r_oit 1 (WBOIT)'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All MBOIT experimental status checks passed."
exit 0
