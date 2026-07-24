#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q '0.27' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
grep -q 'angularRadius' "$ROOT/docs/HDR_SUN_EXPOSURE.md" || grep -q '0.27' "$ROOT/docs/HDR_SUN_EXPOSURE.md"
echo "PASS: hdr sun angular size documented"
