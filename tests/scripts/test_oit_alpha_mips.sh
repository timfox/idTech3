#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -qi 'mip' "$ROOT/docs/TRANSPARENT_TEXTURE_AUTHORING.md"
grep -q 'EDGE_DILATED_STRAIGHT\|OIT_FILTER_EDGE_DILATED' "$ROOT/renderers/vulkan/vk_oit_alpha.h"
echo "OK: mip / dilation guidance"
