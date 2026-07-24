#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'unexposed SceneHDR' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
grep -q 'SceneHDR' "$ROOT/docs/HDR_SUN_EXPOSURE.md"
echo "PASS: hdr sun scene radiance convention"
