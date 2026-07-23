#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q OIT_SOURCE_ALPHA_STRAIGHT "$ROOT/renderers/vulkan/vk_oit_alpha.h"
grep -q OIT_SOURCE_ALPHA_PREMULTIPLIED "$ROOT/renderers/vulkan/vk_oit_alpha.h"
grep -q OIT_SOURCE_ALPHA_UNKNOWN "$ROOT/renderers/vulkan/vk_oit_alpha.h"
echo "OK: source encodings"
