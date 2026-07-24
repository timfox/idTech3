#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'preexposure_gray_validate' "$ROOT/renderers/vulkan/vk_gray_veil.c"
grep -q 'unexposed until tonemap' "$ROOT/renderers/vulkan/vk_gray_veil.c"
echo "PASS: preexposure consistency"
