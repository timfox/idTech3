#!/usr/bin/env bash
# Static gate: OIT anomaly capture on error (r_oitCaptureOnError + vk_oit_certify_note_anomaly).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

CERT_C="$ROOT/renderers/vulkan/vk_oit_certify.c"
POSTFX="$ROOT/renderers/vulkan/vk_postfx_passes.c"

[[ -f "$CERT_C" ]] || fail "missing $CERT_C"

grep -q 'r_oitCaptureOnError' "$CERT_C" || fail 'r_oitCaptureOnError cvar missing in vk_oit_certify.c'
grep -q 'vk_oit_certify_note_anomaly' "$CERT_C" || fail 'vk_oit_certify_note_anomaly definition missing'
grep -q 'oitCapturePending' "$CERT_C" || fail 'anomaly must arm oitCapturePending'

if [[ -f "$POSTFX" ]]; then
	grep -q 'vk_oit_certify_note_anomaly' "$POSTFX" || fail 'postfx must call vk_oit_certify_note_anomaly'
else
	fail "missing $POSTFX"
fi

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT capture-on-error checks passed."
