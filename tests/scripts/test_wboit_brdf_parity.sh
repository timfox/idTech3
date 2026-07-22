#!/usr/bin/env bash
# Static gate: shared Forward+ BRDF eval in WBOIT accum + parity debug cvars.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

EVAL="$ROOT/renderers/vulkan/shaders/glsl/forward_plus_light_eval.glsl"
ACCUM="$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag"
TR="$ROOT/renderers/vulkan/tr_init.c"

[[ -f "$EVAL" ]] || fail 'forward_plus_light_eval.glsl missing'
grep -q 'forward_plus_light_eval.glsl' "$ACCUM" || fail 'oit_accum.frag must include forward_plus_light_eval.glsl'
grep -q 'r_oitLightingDebug' "$TR" || fail 'r_oitLightingDebug not registered'
grep -q 'r_oitParityCompare' "$TR" || fail 'r_oitParityCompare not registered'
grep -Eq 'FpEval_Diffuse_Burley|Diffuse_Burley' "$EVAL" || fail 'Burley diffuse eval missing from include'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT BRDF parity wiring checks passed."
exit 0
