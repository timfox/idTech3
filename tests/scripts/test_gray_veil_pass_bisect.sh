#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'gray_veil_bisect' "$ROOT/renderers/vulkan/vk_gray_veil.c"
grep -q 'LOCAL_EXPOSURE_SHADOW_LIFT' "$ROOT/renderers/vulkan/vk_gray_veil.c"
echo "PASS: gray veil pass bisect"
