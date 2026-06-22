#!/usr/bin/env bash
# Dressi HardSoftRas scaffold checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'vk_dressi_record_pass' "$ROOT/src/renderers/vulkan/extensions/scaffold/vk_dressi.c"
grep -q 'R_Dressi_Init' "$ROOT/src/renderers/vulkan/tr_init.c"
grep -q 'dressi_soft.vert' "$ROOT/scripts/compile_shaders.sh"
grep -q 'dressi_blend.comp' "$ROOT/scripts/compile_shaders.sh"
grep -q 'dressi_status' "$ROOT/src/renderers/vulkan/extensions/scaffold/vk_dressi.c"
test -f "$ROOT/docs/DRESSI.md"
test -f "$ROOT/src/renderers/vulkan/shaders/glsl/dressi/dressi_soft.frag"
test -f "$ROOT/examples/demo_game/mod/demo_dressi.cfg"

echo "test_dressi.sh: ok"
