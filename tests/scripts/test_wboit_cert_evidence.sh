#!/usr/bin/env bash
# Phase 2.6A — evidence-backed live GPU certification infrastructure.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

CERT_H="$ROOT/renderers/vulkan/vk_wboit_production_cert.h"
CERT_C="$ROOT/renderers/vulkan/vk_wboit_production_cert.c"
RB_H="$ROOT/renderers/vulkan/vk_cert_readback.h"
RB_C="$ROOT/renderers/vulkan/vk_cert_readback.c"
MET_H="$ROOT/renderers/vulkan/vk_cert_metrics.h"
MET_C="$ROOT/renderers/vulkan/vk_cert_metrics.c"
LAB="$ROOT/renderers/vulkan/vk_oit_lab.c"
ROUTE="$ROOT/renderers/vulkan/vk_transparency_route.c"
DOC="$ROOT/docs/WBOIT_LIVE_CERTIFICATION.md"

for f in "$CERT_H" "$CERT_C" "$RB_H" "$RB_C" "$MET_H" "$MET_C" "$LAB" "$DOC"; do
	[[ -f "$f" ]] || fail "missing $f"
done
pass "2.6A sources present"

grep -q 'WBOIT_EVIDENCE_GPU_READBACK' "$CERT_H" || fail "GPU_READBACK evidence missing"
grep -q 'WBOIT_EVIDENCE_MANUAL_OVERRIDE' "$CERT_H" || fail "MANUAL_OVERRIDE missing"
grep -q 'wboitCertStageResult_t' "$CERT_H" || fail "stage result struct missing"
grep -q 'r_oitAllowManualCertification' "$CERT_C" || fail "r_oitAllowManualCertification missing"
grep -q 'r_requireWboitCertification' "$CERT_C" || fail "r_requireWboitCertification missing"
grep -q 'MANUAL_OVERRIDE' "$CERT_C" || fail "manual override path missing"
grep -q 'cannot grant WBOIT_PRODUCTION_CERTIFIED\|never grants PRODUCTION' "$CERT_C" || fail "manual≠production policy"
grep -q 'oit_certification_export' "$CERT_C" || fail "export missing"
grep -q 'oit_certification_invalidate' "$CERT_C" || fail "invalidate missing"
grep -q 'render_cert/wboit_certification.json' "$CERT_C" || fail "json path missing"
pass "evidence model + promotion policy"

grep -q 'CERT_RB_OIT_ACCUM' "$RB_H" || fail "OIT accum readback missing"
grep -q 'vk_cert_half_to_float' "$RB_H" || fail "half decode missing"
grep -q 'cert_readback_capture' "$RB_C" || fail "capture cmd missing"
grep -q 'R16_SFLOAT' "$RB_C" || fail "R16 format support missing"
pass "readback"

grep -q 'modifiedEmptyPixels' "$MET_H" || fail "empty pixel metric missing"
grep -q 'vk_cert_metrics_empty_pixels' "$MET_C" || fail "empty pixel impl missing"
grep -q 'additiveRevealageDelta' "$MET_H" || fail "additive metric missing"
pass "metrics"

grep -q 'oit_lab_run' "$LAB" || fail "oit_lab_run missing"
grep -q 'oit_lab_run_group' "$LAB" || fail "oit_lab_run_group missing"
grep -q 'wboit_empty_pixel' "$LAB" || fail "empty pixel case missing"
grep -q 'WBOIT_EVIDENCE_CPU_REFERENCE' "$LAB" || fail "CPU ref must not silently become GPU PASS"
grep -q 'vk_oit_lab_register' "$ROUTE" || fail "lab not registered"
grep -q 'vk_cert_readback_register' "$ROUTE" || fail "readback not registered"
pass "lab + wiring"

grep -q '2.6A\|evidence' "$DOC" || fail "live cert doc must mention 2.6A/evidence"
grep -q 'MANUAL_OVERRIDE\|manual' "$DOC" || fail "doc must describe manual policy"
pass "docs"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All Phase 2.6A evidence / readback / lab gates passed."
