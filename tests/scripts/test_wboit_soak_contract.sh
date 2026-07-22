#!/usr/bin/env bash
# Static gate: WBOIT soak contract (oit_soak_wboit, 120-frame history, CSV export).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

CERT_C="$ROOT/renderers/vulkan/vk_oit_certify.c"

[[ -f "$CERT_C" ]] || fail "missing $CERT_C"

grep -q 'oit_soak_wboit' "$CERT_C" || fail 'oit_soak_wboit command missing'
grep -q 'Cmd_AddCommand.*"oit_soak_wboit"' "$CERT_C" || fail 'Cmd_AddCommand oit_soak_wboit missing'
grep -q 'VK_OIT_SOAK_HISTORY[[:space:]]*120' "$CERT_C" || fail 'VK_OIT_SOAK_HISTORY 120 missing'
grep -q 'oit_soak_.*\.csv' "$CERT_C" || fail 'soak CSV export path missing'
grep -q 'oit_soak_.*\.json' "$CERT_C" || fail 'soak JSON export path missing'
grep -q 'VK_OitSoak_Export' "$CERT_C" || fail 'VK_OitSoak_Export missing'
grep -q 'LIVE_SOAKED' "$CERT_C" || fail 'LIVE_SOAKED level reference missing'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT soak contract checks passed."
