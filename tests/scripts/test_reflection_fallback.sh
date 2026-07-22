#!/usr/bin/env bash
# Foundation Consolidation: reflection hierarchy fallback chain.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/REFLECTION_HIERARCHY.md"
SHR="$ROOT/renderers/vulkan/vk_selective_reflection.c"

[[ -f "$DOC" ]] || fail "missing REFLECTION_HIERARCHY.md"
grep -q 'planar' "$DOC" || fail "doc must mention planar"
grep -q 'SSR' "$DOC" || fail "doc must mention SSR"
grep -q 'probe' "$DOC" || fail "doc must mention probe"
grep -q 'sky' "$DOC" || fail "doc must mention sky"
grep -q 'r_reflectionDebug' "$DOC" || fail "doc must mention r_reflectionDebug"
pass "REFLECTION_HIERARCHY.md chain documented"

grep -q 'r_shrDebug' "$SHR" || fail "r_shrDebug missing (reflection debug baseline)"
pass "selective reflection debug cvar present"

if [[ -f "$ROOT/renderers/vulkan/vk_reflection_hierarchy.c" ]]; then
	grep -q 'r_reflectionDebug' "$ROOT/renderers/vulkan/vk_reflection_hierarchy.c" || fail "r_reflectionDebug missing"
	grep -q 'reflection_hierarchy_status' "$ROOT/renderers/vulkan/vk_reflection_hierarchy.c" || fail "reflection_hierarchy_status missing"
	grep -q 'VK_REFLECTION_SOURCE_SSR' "$ROOT/renderers/vulkan/vk_reflection_hierarchy.h" || fail "SSR source enum missing"
	pass "vk_reflection_hierarchy module present"
else
	grep -q 'reflection_hierarchy_status' "$DOC" || fail "doc must mention reflection_hierarchy_status"
	pass "reflection hierarchy documented (source optional)"
fi

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All reflection fallback checks passed."
