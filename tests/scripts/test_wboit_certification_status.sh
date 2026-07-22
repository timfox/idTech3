#!/usr/bin/env bash
# Static gate: oit_certification_status levels; docs must say static alone is not certification.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

CERT_C="$ROOT/renderers/vulkan/vk_oit_certify.c"
CERT_H="$ROOT/renderers/vulkan/vk_oit_certify.h"
CERT_DOC="$ROOT/docs/WBOIT_GPU_CERTIFICATION.md"

[[ -f "$CERT_C" ]] || fail "missing $CERT_C"
[[ -f "$CERT_H" ]] || fail "missing $CERT_H"
[[ -f "$CERT_DOC" ]] || fail "missing $CERT_DOC"

grep -q 'oit_certification_status' "$CERT_C" || fail 'oit_certification_status command missing'
grep -q 'Cmd_AddCommand.*"oit_certification_status"' "$CERT_C" || fail 'Cmd_AddCommand oit_certification_status missing'
grep -q 'STATIC_GATES alone is NOT' "$CERT_C" || fail 'runtime must warn static alone is not cert'

grep -q 'VK_OIT_CERT_STATIC_GATES' "$CERT_H" || fail 'STATIC_GATES enum missing'
grep -q 'VK_OIT_CERT_SPINE_1_1' "$CERT_H" || fail 'SPINE_1_1 enum missing'

grep -Eiq 'static.*(scripts|gates).*alone.*(do not|NOT).*certif' "$CERT_DOC" || \
	fail 'WBOIT_GPU_CERTIFICATION.md must state static scripts alone do NOT certify'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT certification status checks passed."
