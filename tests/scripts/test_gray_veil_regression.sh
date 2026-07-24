#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'vk_gray_veil_register' "$ROOT/renderers/vulkan/tr_init.c"
grep -q 'liftMax' "$ROOT/renderers/vulkan/shaders/glsl/gamma.frag"
grep -q 'r_localExposure", "0"' "$ROOT/renderers/vulkan/vk_skybox_hdr.c"
grep -q 'r_tonemap' "$ROOT/renderers/vulkan/vk_skybox_hdr.c"
test -f "$ROOT/config/gray_veil_neutral_ref.cfg"
test -f "$ROOT/docs/GRAY_VEIL.md"
echo "PASS: gray veil regression"
