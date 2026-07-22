#!/usr/bin/env bash
# Deferred Honesty — eligibility enums, reasons, debug tint, Forward+ fallback.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

HH="$ROOT/renderers/vulkan/vk_deferred_honesty.h"
H="$ROOT/renderers/vulkan/vk_deferred_honesty.c"

for sym in DEFERRED_ELIGIBLE_FULL DEFERRED_ELIGIBLE_APPROXIMATE DEFERRED_FORWARD_FALLBACK \
	DEFERRED_UNSUPPORTED DEFERRED_DEBUG_FORCED \
	DEFERRED_REASON_NO_BASE_COLOR_EXPORT DEFERRED_REASON_MULTISTAGE_CLASSIC_SHADER \
	DEFERRED_REASON_TRANSMISSION_OR_REFRACTION DEFERRED_REASON_PBR_NATIVE \
	DEFERRED_REASON_CLASSIC_TRANSLATED; do
	grep -q "$sym" "$HH" || fail "missing $sym"
done
pass "eligibility + reason enums"

grep -q 'r_deferredEligibilityDebug' "$H" || fail "r_deferredEligibilityDebug missing"
grep -q 'R_DeferredEligibility_DebugColor' "$H" || fail "debug color helper missing"
grep -q 'R_DeferredHonesty_WantsDeferredPath' "$H" || fail "WantsDeferredPath missing"
pass "eligibility debug + gate helpers"

grep -q 'r_deferredEligibilityDebug' "$ROOT/renderers/vulkan/tr_shade.c" || \
	fail "tr_shade must drive eligibility tint"
grep -q 'pbrDebugMode.w' "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" || \
	fail "gen_frag must tint from pbrDebugMode.w"
pass "eligibility tint wired"

grep -q 'material_translate_status' "$H" || fail "material_translate_status missing"
pass "material_translate_status command"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All deferred eligibility checks passed."
