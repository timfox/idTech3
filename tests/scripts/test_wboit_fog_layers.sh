#!/usr/bin/env bash
# Static gate: fog density in oit_accum.frag + demo_wboit_fog_layers.cfg.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

SHADER="$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag"
DEMO="$ROOT/config/demo_wboit_fog_layers.cfg"

[[ -f "$SHADER" ]] || fail "missing $SHADER"
[[ -f "$DEMO" ]] || fail "missing $DEMO"

grep -q 'fogDensity' "$SHADER" || fail 'oit_accum.frag must reference fogDensity'
grep -q 'fogMode' "$SHADER" || fail 'oit_accum.frag must reference fogMode'
grep -q 'exp.*fogDensity' "$SHADER" || fail 'oit_accum.frag must compute T from fogDensity'

grep -q 'vulkan_overlay_oit_clustered.cfg' "$DEMO" || fail 'demo must exec oit clustered overlay'
grep -Eq 'seta[[:space:]]+r_oitFogMode[[:space:]]+1' "$DEMO" || fail 'demo must set r_oitFogMode 1'
grep -Eq 'seta[[:space:]]+r_oitFogDensity' "$DEMO" || fail 'demo must set r_oitFogDensity'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT fog layers checks passed."
