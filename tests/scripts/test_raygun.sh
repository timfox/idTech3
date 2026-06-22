#!/usr/bin/env bash
# Raygun RT scaffolding checks (arXiv:2001.09792)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'vk_raygun_record_pass' "$ROOT/src/renderers/vulkan/extensions/rtx/vk_raygun.c"
grep -q 'r_raygun_fxaa' "$ROOT/src/renderers/vulkan/extensions/rtx/vk_raygun.c"
grep -q 'raygun/raygun.rgen' "$ROOT/scripts/compile_shaders.sh"
test -f "$ROOT/docs/RAYGUN.md"
test -f "$ROOT/src/renderers/vulkan/shaders/glsl/raygun/raygun.rchit"
test -f "$ROOT/examples/demo_game/mod/demo_raygun.cfg"
grep -q 'write_vk_raygun_spirv_inc' "$ROOT/scripts/compile_shaders.sh"
grep -q 'vk_raygun_init' "$ROOT/src/renderers/vulkan/vk_init_device.c"
grep -q 'r_raygun' "$ROOT/src/renderers/vulkan/tr_local.h"

echo "test_raygun.sh: ok"
