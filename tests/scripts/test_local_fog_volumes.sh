#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'local_fog_status' "$ROOT/renderers/vulkan/vk_world_feature_support.c"
grep -q 'localFogVolume_t' "$ROOT/renderers/vulkan/vk_world_presentation.h"
echo "PASS: local fog volumes"
