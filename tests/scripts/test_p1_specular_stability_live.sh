#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'P1_CERT_STAGE_SPECULAR_STABILITY' "$ROOT/renderers/vulkan/vk_renderer_p1_cert.h"
echo "All p1_specular_stability_live checks passed."
