#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'sun_tonemap_status' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
grep -q 'Tonemap_Filmic\|ACESFilm' "$ROOT/renderers/vulkan/shaders/glsl/gamma.frag"
echo "PASS: shared tonemap path for sun"
