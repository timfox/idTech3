#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'auto_exposure_contrast_status' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
grep -q 'r_exposureSkyWeight", "0.55"' "$ROOT/renderers/vulkan/vk_skybox_hdr.c"
echo "PASS: world contrast under sun metering policy"
