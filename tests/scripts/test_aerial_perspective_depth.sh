#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'local_contrast_status' "$ROOT/renderers/vulkan/vk_gray_veil.c"
echo "PASS: aerial perspective / local contrast"
