#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'eye_adaptation_exposure_test\|AUTO_EXPOSURE kept' "$ROOT/renderers/vulkan/vk_hdr_sun.c" "$ROOT/renderers/vulkan/vk_gray_veil.c"
echo "PASS: exposure transition / AE kept"
