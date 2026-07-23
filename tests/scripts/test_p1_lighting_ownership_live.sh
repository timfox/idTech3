#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'P1_CERT_STAGE_LIGHTING_OWNERSHIP' "$ROOT/renderers/vulkan/vk_renderer_p1_cert.h"
echo "All p1_lighting_ownership_live checks passed."
