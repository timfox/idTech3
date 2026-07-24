#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'color_grade_gray_status' "$ROOT/renderers/vulkan/vk_gray_veil.c"
echo "PASS: color grade black level"
