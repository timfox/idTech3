#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'terrain_patch_status' "$ROOT/renderers/vulkan/vk_world_feature_support.c"
grep -q 'terrainPatch_t' "$ROOT/renderers/vulkan/vk_world_presentation.h"
echo "PASS: terrain patch LOD scaffold"
