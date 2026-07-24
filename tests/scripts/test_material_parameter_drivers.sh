#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'material_driver_status' "$ROOT/renderers/vulkan/vk_world_feature_support.c"
grep -q 'materialParameterDriver_t' "$ROOT/renderers/vulkan/vk_world_presentation.h"
echo "PASS: material parameter drivers"
