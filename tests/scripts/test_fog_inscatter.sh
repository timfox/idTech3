#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'inscatter=0' "$ROOT/renderers/vulkan/vk_gray_veil.c"
echo "PASS: fog inscatter clear policy"
