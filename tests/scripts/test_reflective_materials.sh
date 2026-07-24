#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'reflectionMaterialExtension_t' "$ROOT/renderers/vulkan/vk_world_presentation.h"
echo "PASS: reflective material extension type"
