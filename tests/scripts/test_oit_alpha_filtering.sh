#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'OIT_FILTER_STRAIGHT_SOURCE\|OIT_FILTER_PREMULTIPLIED_SOURCE\|transparentFilterMode\|filterMode' "$ROOT/renderers/vulkan/vk_oit_alpha.h"
grep -q 'r_alphaFilterDebug' "$ROOT/renderers/vulkan/vk_oit_alpha.c"
grep -q 'filtering\|Filtering\|mip' "$ROOT/docs/TRANSPARENT_TEXTURE_AUTHORING.md"
echo "OK: alpha filtering docs/API"
