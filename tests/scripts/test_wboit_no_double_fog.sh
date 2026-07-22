#!/usr/bin/env bash
# Static gate: no double fog — doc + shader fogMode>=1 path.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

FOG_DOC="$ROOT/docs/WBOIT_FOG_LAYERS.md"
SHADER="$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag"
CERT_C="$ROOT/renderers/vulkan/vk_oit_certify.c"

[[ -f "$FOG_DOC" ]] || fail "missing $FOG_DOC"
[[ -f "$SHADER" ]] || fail "missing $SHADER"

if grep -q 'doubleFogPrevention' "$FOG_DOC"; then
	pass 'WBOIT_FOG_LAYERS.md mentions doubleFogPrevention'
elif grep -qi 'no second' "$FOG_DOC"; then
	pass 'WBOIT_FOG_LAYERS.md mentions no second fog'
else
	fail 'WBOIT_FOG_LAYERS.md must document doubleFogPrevention or no second fog'
fi

grep -q 'fogMode >= 1' "$SHADER" || fail 'oit_accum.frag must gate fog on fogMode >= 1'
grep -q 'doubleFogPrevention\|no second full-screen fog' "$CERT_C" || \
	fail 'vk_oit_certify.c oit_fog_status must mention double fog prevention'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT no-double-fog checks passed."
