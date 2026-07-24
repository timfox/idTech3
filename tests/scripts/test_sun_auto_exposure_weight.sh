#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'skyWeight' "$ROOT/renderers/vulkan/shaders/glsl/postfx/luminance.comp"
grep -q 'r_exposureSkyWeight' "$ROOT/renderers/vulkan/vk_skybox_hdr.c"
echo "PASS: sky exposure weight"
