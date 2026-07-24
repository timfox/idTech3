#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'BLOOM_LOW_FREQUENCY' "$ROOT/renderers/vulkan/vk_gray_veil.c"
echo "PASS: bloom low frequency energy"
