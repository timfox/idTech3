#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'oit_alpha_validate' "$ROOT/renderers/vulkan/vk_oit_alpha.c"
grep -q 'OIT_ALPHA_EDGE_CERTIFIED' "$ROOT/renderers/vulkan/vk_oit_alpha.c"
grep -q 'oit_alpha_status' "$ROOT/renderers/vulkan/vk_transparency_route.c" || grep -q 'oit_alpha_status' "$ROOT/renderers/vulkan/vk_oit_alpha.c"
echo "OK: runtime alpha validation"
