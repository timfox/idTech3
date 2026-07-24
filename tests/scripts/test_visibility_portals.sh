#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'visibility_portal_status' "$ROOT/renderers/vulkan/vk_world_feature_support.c"
grep -q 'visibilityPortal_t' "$ROOT/renderers/vulkan/vk_world_presentation.h"
echo "PASS: visibility portals"
