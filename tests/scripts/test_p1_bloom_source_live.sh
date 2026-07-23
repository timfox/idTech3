#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'P1_CERT_STAGE_BLOOM_SOURCE' "$ROOT/renderers/vulkan/vk_renderer_p1_cert.h"
grep -q 'bloom_source' "$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
grep -q 'P1_CERT_BLOOM_SOURCE' "$ROOT/docs/BLOOM_GPU_CERTIFICATION.md"
echo "All p1_bloom_source_live checks passed."
