#!/usr/bin/env bash
# Foundation Consolidation: indirect lighting sources + probe contract.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/INDIRECT_LIGHTING.md"

[[ -f "$DOC" ]] || fail "missing INDIRECT_LIGHTING.md"
grep -q 'lightmap' "$DOC" || fail "doc must mention lightmaps"
grep -q 'deluxe' "$DOC" || fail "doc must mention deluxe maps"
grep -q 'GTAO\|AO' "$DOC" || fail "doc must mention GTAO/AO"
grep -q 'r_indirectDebug' "$DOC" || fail "doc must mention r_indirectDebug"
grep -q 'indirect_light_status' "$DOC" || fail "doc must mention indirect_light_status"
pass "INDIRECT_LIGHTING.md symbols present"

if [[ -f "$ROOT/renderers/vulkan/vk_indirect_light.c" ]]; then
	grep -q 'r_indirectDebug' "$ROOT/renderers/vulkan/vk_indirect_light.c" || fail "r_indirectDebug missing"
	grep -q 'indirect_light_status' "$ROOT/renderers/vulkan/vk_indirect_light.c" || fail "indirect_light_status missing"
	grep -q 'vkIrradianceProbe_t' "$ROOT/renderers/vulkan/vk_indirect_light.h" || fail "probe struct missing"
	pass "vk_indirect_light module present"
else
	grep -q 'vk_indirect_light' "$DOC" || fail "doc must reference vk_indirect_light"
	pass "indirect light documented (source optional)"
fi

grep -q 'lightmap' "$ROOT/renderers/vulkan/tr_bsp.c" || fail "BSP lightmap path must exist"
pass "classic lightmap wiring present"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All indirect light parity checks passed."
