#!/usr/bin/env bash
# Static gate: fog ownership (oit_fog_status, r_oitFogMode, WBOIT_FOG_LAYERS pass order).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

CERT_C="$ROOT/renderers/vulkan/vk_oit_certify.c"
FOG_DOC="$ROOT/docs/WBOIT_FOG_LAYERS.md"

[[ -f "$CERT_C" ]] || fail "missing $CERT_C"
[[ -f "$FOG_DOC" ]] || fail "missing $FOG_DOC"

grep -q 'oit_fog_status' "$CERT_C" || fail 'oit_fog_status command missing'
grep -q 'Cmd_AddCommand.*"oit_fog_status"' "$CERT_C" || fail 'Cmd_AddCommand oit_fog_status missing'
grep -q 'r_oitFogMode' "$CERT_C" || fail 'r_oitFogMode cvar missing'

grep -q 'opaque.*volumetric' "$FOG_DOC" || fail 'pass order must mention opaque + volumetrics'
grep -q 'WBOIT fogged-lit accum' "$FOG_DOC" || fail 'pass order must mention WBOIT fogged-lit accum'
grep -q 'resolve over fogged opaque' "$FOG_DOC" || fail 'pass order must mention resolve over fogged opaque'
grep -q 'weapon' "$FOG_DOC" || fail 'pass order must mention weapon'
grep -q 'bloom' "$FOG_DOC" || fail 'pass order must mention bloom'
grep -q 'tonemap' "$FOG_DOC" || fail 'pass order must mention tonemap'

ROUTE_H="$ROOT/renderers/vulkan/vk_transparency_route.h"
[[ -f "$ROUTE_H" ]] || fail "missing $ROUTE_H"
grep -q 'TRANSPARENCY_SURFACE' "$ROUTE_H" || fail 'TRANSPARENCY_SURFACE macro missing'
grep -q 'TRANSPARENCY_PARTICLE' "$ROUTE_H" || fail 'TRANSPARENCY_PARTICLE macro missing'
grep -q 'TRANSPARENCY_VOLUME_PROXY' "$ROUTE_H" || fail 'TRANSPARENCY_VOLUME_PROXY macro missing'
grep -q 'TRANSPARENCY_REFRACTIVE' "$ROUTE_H" || fail 'TRANSPARENCY_REFRACTIVE macro missing'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT fog ownership checks passed."
