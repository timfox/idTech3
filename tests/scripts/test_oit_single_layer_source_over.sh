#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'r_oitSingleLayerCompare' "$ROOT/renderers/vulkan/vk_oit_alpha.c"
grep -q 'vk_oit_reference_source_over\|source_over' "$ROOT/renderers/vulkan/vk_oit_alpha.c" "$ROOT/tests/unit/test_oit_alpha_normalize.c"
grep -q 'alpha0 preserves bg' "$ROOT/tests/unit/test_oit_alpha_normalize.c"
echo "OK: single-layer source-over"
