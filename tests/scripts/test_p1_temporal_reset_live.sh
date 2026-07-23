#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'P1_CERT_STAGE_TEMPORAL_RESET' "$ROOT/renderers/vulkan/vk_renderer_p1_cert.h"
grep -q 'temporal_reset' "$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
echo "All p1_temporal_reset_live checks passed."
