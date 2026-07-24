#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'color_correction_status' "$ROOT/renderers/vulkan/vk_world_feature_support.c"
grep -q 'colorCorrectionVolume_t' "$ROOT/renderers/vulkan/vk_world_presentation.h"
echo "PASS: color correction volumes"
