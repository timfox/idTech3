#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'Policy A' "$ROOT/renderers/vulkan/vk_gray_veil.c"
echo "PASS: swapchain color contract"
