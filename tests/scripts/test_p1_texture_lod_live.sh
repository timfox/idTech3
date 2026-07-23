#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'P1_CERT_STAGE_TEXTURE_LOD' "$ROOT/renderers/vulkan/vk_renderer_p1_cert.h"
echo "All p1_texture_lod_live checks passed."
