#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'Fresnel' "$ROOT/renderers/vulkan/vk_water_presentation.c"
grep -q 'waterMaterial_t' "$ROOT/renderers/vulkan/vk_world_presentation.h"
echo "PASS: water fresnel design"
