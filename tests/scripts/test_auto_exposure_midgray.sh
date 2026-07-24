#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'r_exposure_auto_target", "0.22"' "$ROOT/renderers/vulkan/vk_skybox_hdr.c"
grep -q 'r_autoExposure_min", "0.55"' "$ROOT/renderers/vulkan/vk_skybox_hdr.c"
echo "PASS: auto exposure midgray outdoor policy"
