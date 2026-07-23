#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'P1_CERT_STAGE_EDGE' "$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
grep -q 'vk_cert_metrics_edge' "$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
echo "All p1_edge_quality_live checks passed."
