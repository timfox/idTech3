#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'P1_CERT_STAGE_SMAA' "$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
echo "All p1_smaa_live checks passed."
