#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'EMISSIVE_SCALED_BY_SURFACE_ALPHA' "$ROOT/renderers/vulkan/vk_oit_alpha.h"
grep -q 'EMISSIVE_INDEPENDENT_OF_SURFACE_ALPHA' "$ROOT/renderers/vulkan/vk_oit_alpha.h"
grep -q 'EMISSIVE_ADDITIVE_ROUTE' "$ROOT/renderers/vulkan/vk_oit_alpha.h"
echo "OK: emissive opacity policies"
