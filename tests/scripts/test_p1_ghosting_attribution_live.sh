#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'P1_CERT_STAGE_GHOSTING' "$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
grep -q 'GHOSTING' "$ROOT/docs/GHOSTING_ATTRIBUTION.md"
echo "All p1_ghosting_attribution_live checks passed."
