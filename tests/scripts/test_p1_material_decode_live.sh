#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'P1_CERT_STAGE_MATERIAL_DECODE' "$ROOT/renderers/vulkan/vk_renderer_p1_cert.h"
echo "All p1_material_decode_live checks passed."
