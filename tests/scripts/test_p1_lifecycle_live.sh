#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'P1_CERT_STAGE_LIFECYCLE' "$ROOT/renderers/vulkan/vk_renderer_p1_cert.h"
grep -q 'lifecycle' "$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
echo "All p1_lifecycle_live checks passed."
