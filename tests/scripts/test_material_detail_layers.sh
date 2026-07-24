#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'material_detail_status' "$ROOT/renderers/vulkan/vk_world_feature_support.c"
echo "PASS: material detail layers"
