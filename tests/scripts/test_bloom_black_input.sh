#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'black source must' "$ROOT/renderers/vulkan/vk_gray_veil.c"
echo "PASS: bloom black input policy"
