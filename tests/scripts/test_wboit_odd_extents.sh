#!/usr/bin/env bash
# Static gate: odd render extents + tile clamp wiring for WBOIT.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

SELF="$ROOT/tests/scripts/test_wboit_odd_extents.sh"
DOC="$ROOT/docs/MOMENT_OIT_STOCHASTIC_ALPHA.md"
EXTENTS=( '1919x1079' '1921x1081' '1279x719' '1281x721' )

found=0
for ext in "${EXTENTS[@]}"; do
	if grep -q "$ext" "$DOC" 2>/dev/null || grep -q "$ext" "$SELF"; then
		found=$((found + 1))
	fi
done
if [[ $found -eq ${#EXTENTS[@]} ]]; then
	pass "odd extent list present in MOMENT doc or this script"
else
	fail "must list extents: ${EXTENTS[*]}"
fi

grep -q 'r_oitExtentDebug' "$ROOT/renderers/vulkan/tr_init.c" || fail 'r_oitExtentDebug not registered'
grep -q 'clamp' "$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag" || fail 'oit_accum.frag missing clamp'
grep -Eq 'Odd extents|clamp.*tile' "$ROOT/renderers/vulkan/shaders/glsl/forward_plus_light_eval.glsl" || \
	fail 'forward_plus_light_eval.glsl missing odd-extent tile clamp comments'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT odd-extent wiring checks passed."
exit 0
