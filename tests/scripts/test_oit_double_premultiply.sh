#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'r_oitFaultDoublePremultiply' "$ROOT/renderers/vulkan/vk_oit_alpha.c"
! grep -qE 'associatedRadiance \* alpha \* w|litRgb \* alpha \* alpha' "$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag"
echo "OK: double-premultiply guard"
