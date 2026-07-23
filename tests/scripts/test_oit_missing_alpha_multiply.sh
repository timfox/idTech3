#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'r_oitFaultSkipAlphaMultiply' "$ROOT/renderers/vulkan/vk_oit_alpha.c"
grep -q 'litRgb \* alpha, alpha ) \* w' "$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag"
echo "OK: alpha multiply present"
