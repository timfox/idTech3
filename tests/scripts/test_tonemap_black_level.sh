#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'tonemap_black_status' "$ROOT/renderers/vulkan/vk_gray_veil.c"
grep -q 'r_tonemap", "3"' "$ROOT/renderers/vulkan/vk_skybox_hdr.c" || grep -q 'r_tonemap' "$ROOT/renderers/vulkan/vk_skybox_hdr.c"
echo "PASS: tonemap black level"
