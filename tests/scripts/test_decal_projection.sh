#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'decal_status' "$ROOT/renderers/vulkan/vk_world_feature_support.c"
test -f "$ROOT/renderers/vulkan/vk_deferred_decals.c"
echo "PASS: decal projection"
