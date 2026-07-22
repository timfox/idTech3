#!/usr/bin/env bash
# Static gate: OIT fault-injection cvars + allocation-failure fallback path.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

TR="$ROOT/renderers/vulkan/tr_init.c"
POSTFX="$ROOT/renderers/vulkan/vk_postfx_passes.c"

for cvar in \
	r_oitForceAllocationFailure \
	r_oitForceExtentMismatch \
	r_oitForceGenerationMismatch \
	r_oitForceSkipClear \
	r_oitForceDoubleResolve \
	r_oitForceInvalidAccum \
	r_oitForceClusterMismatch
do
	grep -q "$cvar" "$TR" || fail "$cvar not registered in tr_init.c"
done
pass 'all r_oitForce* cvars registered'

grep -q 'r_oitForceAllocationFailure' "$POSTFX" || fail 'vk_oit_resources_ready must check r_oitForceAllocationFailure'
grep -q 'forced allocation failure' "$POSTFX" || fail 'allocation failure fallback message missing'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All OIT fault-injection wiring checks passed."
exit 0
