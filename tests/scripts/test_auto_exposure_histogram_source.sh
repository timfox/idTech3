#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'pre-bloom/UI' "$ROOT/renderers/vulkan/vk_gray_veil.c"
grep -q 'hdrColorTexture' "$ROOT/renderers/vulkan/shaders/glsl/postfx/luminance.comp"
echo "PASS: AE histogram source"
