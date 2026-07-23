#!/usr/bin/env bash
# Static gate: IQ edge metrics symbols/hooks.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

grep -q 'vk_cert_metrics_edge' "$ROOT/renderers/vulkan/vk_cert_metrics.h" || fail 'edge metrics API'
grep -q 'spreadWidthPx' "$ROOT/renderers/vulkan/vk_cert_metrics.h" || fail 'spreadWidthPx'
grep -q 'vk_cert_metrics_edge' "$ROOT/renderers/vulkan/vk_iq_lab.c" || fail 'lab uses edge metrics'
grep -q 'P1_CERT_STAGE_EDGE' "$ROOT/renderers/vulkan/vk_iq_lab.c" || fail 'edge stage'

[[ $failures -eq 0 ]] || { echo "$failures failed"; exit 1; }
echo "All iq_edge_metrics checks passed."
