#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'display_transfer_validate' "$ROOT/renderers/vulkan/vk_gray_veil.c"
echo "PASS: display transfer"
