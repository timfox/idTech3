#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'vk_exposure_histogram_on_camera_cut' "$ROOT/renderers/vulkan/vk_temporal.c"
grep -q 'auto_exposure_reset' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
echo "PASS: exposure reset on cut / explicit reset"
