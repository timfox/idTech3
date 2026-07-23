#!/usr/bin/env bash
# Static gate: IQ firefly metrics symbols/hooks.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

grep -q 'vk_cert_metrics_firefly' "$ROOT/renderers/vulkan/vk_cert_metrics.h" || fail 'firefly metrics API'
grep -q 'candidateCount' "$ROOT/renderers/vulkan/vk_cert_metrics.h" || fail 'candidateCount'
grep -q 'vk_cert_metrics_firefly' "$ROOT/renderers/vulkan/vk_iq_lab.c" || fail 'lab uses firefly metrics'
grep -q 'P1_CERT_STAGE_BLOOM_FIREFLY' "$ROOT/renderers/vulkan/vk_iq_lab.c" || fail 'firefly stage'

[[ $failures -eq 0 ]] || { echo "$failures failed"; exit 1; }
echo "All iq_firefly_metrics checks passed."
