#!/usr/bin/env bash
# Raygun RT scaffolding checks (arXiv:2001.09792)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

RAYGUN="$(idtech3_file renderers/vulkan/extensions/rtx/vk_raygun.c src/renderers/vulkan/extensions/rtx/vk_raygun.c)"
RCHIT="$(idtech3_file renderers/vulkan/shaders/glsl/raygun/raygun.rchit src/renderers/vulkan/shaders/glsl/raygun/raygun.rchit)"
VK_INIT="$(idtech3_file renderers/vulkan/vk_init_device.c src/renderers/vulkan/vk_init_device.c)"
TR_LOCAL="$(idtech3_file renderers/vulkan/tr_local.h src/renderers/vulkan/tr_local.h)"

grep -q 'vk_raygun_record_pass' "$RAYGUN"
grep -q 'r_raygun_fxaa' "$RAYGUN"
grep -q 'RAYGUN_StateString' "$RAYGUN"
grep -q 'waiting: shared RTX TLAS not ready' "$RAYGUN"
grep -q 'Hybrid1 has RT path priority' "$RAYGUN"
grep -q 'raygun/raygun.rgen' "$ROOT/scripts/compile_shaders.sh"
test -f "$ROOT/docs/RAYGUN.md"
test -f "$RCHIT"
test -f "$ROOT/examples/demo_game/mod/demo_raygun.cfg"
grep -q 'write_vk_raygun_spirv_inc' "$ROOT/scripts/compile_shaders.sh"
grep -q 'vk_raygun_init' "$VK_INIT"
grep -q 'r_raygun' "$TR_LOCAL"

echo "test_raygun.sh: ok"
