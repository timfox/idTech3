#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'exposure_volume_status' "$ROOT/renderers/vulkan/vk_exposure_volumes.c"
grep -q 'worldExposureSettings_t' "$ROOT/renderers/vulkan/vk_world_presentation.h"
echo "PASS: exposure volumes"
