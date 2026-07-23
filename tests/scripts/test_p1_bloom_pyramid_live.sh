#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'P1_CERT_STAGE_BLOOM_PYRAMID' "$ROOT/renderers/vulkan/vk_renderer_p1_cert.h"
grep -q 'bloom_pyramid' "$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
echo "All p1_bloom_pyramid_live checks passed."
