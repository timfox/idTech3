#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'viewmodel_lighting_status' "$ROOT/renderers/vulkan/vk_world_feature_support.c"
grep -q 'viewmodelLightingState_t' "$ROOT/renderers/vulkan/vk_world_presentation.h"
echo "PASS: viewmodel lighting"
