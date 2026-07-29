#!/usr/bin/env bash
# Gate: WBOIT/MBOIT live GPU image-diff certification wiring.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

CERT_H="$ROOT/renderers/vulkan/vk_wboit_production_cert.h"
CERT_C="$ROOT/renderers/vulkan/vk_wboit_production_cert.c"
MET_H="$ROOT/renderers/vulkan/vk_cert_metrics.h"
MET_C="$ROOT/renderers/vulkan/vk_cert_metrics.c"
LAB="$ROOT/renderers/vulkan/vk_oit_lab.c"
DOC="$ROOT/docs/COLOR_PIPELINE.md"

grep -q 'WBOIT_EVIDENCE_GPU_IMAGE_DIFF' "$CERT_H" || fail "GPU image-diff evidence enum missing"
grep -q 'GPU_IMAGE_DIFF' "$CERT_C" || fail "GPU image-diff evidence name missing"
grep -q 'vk_cert_metrics_image_diff_passes' "$MET_H" || fail "image-diff acceptance API missing"
grep -q 'rmseLimit' "$MET_C" || fail "image-diff thresholds missing"
grep -q 'OIT_Lab_BuildSingleLayerReference' "$LAB" || fail "live OIT image reference builder missing"
grep -q 'WBOIT_EVIDENCE_GPU_IMAGE_DIFF' "$LAB" || fail "WBOIT lab must record GPU image-diff evidence"
grep -q 'OIT_LAB_EVAL_MBOIT_SINGLE' "$LAB" || fail "MBOIT live image-diff eval missing"
grep -q 'mboit_image_diff_status' "$LAB" || fail "MBOIT image-diff status command missing"
grep -q 'mboit_compare' "$LAB" || fail "MBOIT lab case missing"
grep -q 'wboit_certification/2.7-image-diff' "$CERT_C" || fail "cert export schema must identify image-diff evidence"
grep -q 'GPU image-diff' "$DOC" || fail "COLOR_PIPELINE.md must document GPU image-diff certification"
grep -q 'mboit_image_diff_status' "$DOC" || fail "COLOR_PIPELINE.md must document MBOIT image-diff status"

pass "OIT live GPU image-diff certification wiring"
