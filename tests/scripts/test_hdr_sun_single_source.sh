#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'SUN_SOURCE_CUBEMAP' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
grep -q 'sun_contributor_validate' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
echo "PASS: hdr sun single source policy"
