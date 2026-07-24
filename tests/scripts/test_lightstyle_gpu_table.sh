#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'GPU table' "$ROOT/renderers/vulkan/vk_world_feature_support.c"
grep -q 'lightStyle_t' "$ROOT/renderers/vulkan/vk_world_presentation.h"
echo "PASS: lightstyle GPU table design"
