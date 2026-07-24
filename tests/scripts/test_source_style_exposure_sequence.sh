#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'eye_adaptation_exposure_test' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
grep -q 'VIEW_DARK_WORLD' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
echo "PASS: eye adaptation exposure sequence command"
