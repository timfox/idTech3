#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'P1_CERT_STAGE_GBUFFER_QUANT' "$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
grep -q 'vk_cert_metrics_quantization' "$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
echo "All p1_gbuffer_quantization_live checks passed."
