#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'r_sunDiffraction", "0"' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
grep -q 'isotropic' "$ROOT/docs/HDR_SUN_EXPOSURE.md" || grep -q 'Diffraction' "$ROOT/docs/HDR_SUN_EXPOSURE.md"
echo "PASS: bloom isotropic / diffraction off by default"
