#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'OIT_SAMPLE_FLAG_ZERO_ALPHA_RGB' "$ROOT/renderers/vulkan/vk_oit_alpha.h"
grep -q 'zeroAlphaColored' "$ROOT/renderers/vulkan/vk_oit_alpha.c"
grep -q 'r_transparentEdgePolicy' "$ROOT/renderers/vulkan/vk_oit_alpha.c"
echo "OK: zero-alpha RGB diagnostics"
