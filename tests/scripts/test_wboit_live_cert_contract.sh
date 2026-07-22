#!/usr/bin/env bash
# Static gate: WBOIT live cert runner contract (oit_certify_wboit, B0 cases, cert levels).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

CERT_C="$ROOT/renderers/vulkan/vk_oit_certify.c"
CERT_DOC="$ROOT/docs/WBOIT_GPU_CERTIFICATION.md"

[[ -f "$CERT_C" ]] || fail "missing $CERT_C"
[[ -f "$CERT_DOC" ]] || fail "missing $CERT_DOC"

grep -q 'oit_certify_wboit' "$CERT_C" || fail 'vk_oit_certify.c must define oit_certify_wboit'
grep -q 'Cmd_AddCommand.*"oit_certify_wboit"' "$CERT_C" || fail 'Cmd_AddCommand oit_certify_wboit missing'
grep -q '"B0a"' "$CERT_C" || fail 'B0a case missing'
grep -q '"B0b"' "$CERT_C" || fail 'B0b case missing'
grep -q '"B0c"' "$CERT_C" || fail 'B0c case missing'
grep -q 'begin|next|repeat|pass|fail' "$CERT_C" || fail 'certify subcommands missing'

for level in STATIC_GATES LIVE_BASIC LIVE_FULL LIVE_SOAKED SPINE_1_1_CERTIFIED; do
	grep -q "$level" "$CERT_DOC" || fail "WBOIT_GPU_CERTIFICATION.md missing level $level"
done

grep -qi 'static.*alone.*NOT' "$CERT_DOC" || fail 'doc must state static scripts alone do NOT certify'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT live cert contract checks passed."
