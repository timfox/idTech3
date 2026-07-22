#!/usr/bin/env bash
# Static gate: WBOIT lifecycle state machine + fault-injection cvars.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

TR="$ROOT/renderers/vulkan/tr_init.c"
VK_H="$ROOT/renderers/vulkan/vk.h"
CHECK="$ROOT/scripts/oit_corruption_check.sh"

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
pass "force-fault cvars registered"

grep -q 'oitFrameState' "$VK_H" || fail 'oitFrameState missing from vk.h'
pass 'oitFrameState in vk.h'

[[ -f "$CHECK" ]] || fail 'scripts/oit_corruption_check.sh missing'
pass 'oit_corruption_check.sh present'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT lifecycle wiring checks passed."
exit 0
