#!/usr/bin/env bash
# Dressi HardSoftRas scaffold checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

DRESSI="$(idtech3_file renderers/vulkan/extensions/scaffold/vk_dressi.c src/renderers/vulkan/extensions/scaffold/vk_dressi.c)"
TR_INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"
FRAG="$(idtech3_file renderers/vulkan/shaders/glsl/dressi/dressi_soft.frag src/renderers/vulkan/shaders/glsl/dressi/dressi_soft.frag)"

grep -q 'vk_dressi_record_pass' "$DRESSI"
grep -q 'R_Dressi_Init' "$TR_INIT"
grep -q 'dressi_soft.vert' "$ROOT/scripts/compile_shaders.sh"
grep -q 'dressi_blend.comp' "$ROOT/scripts/compile_shaders.sh"
grep -q 'dressi_status' "$DRESSI"
test -f "$ROOT/docs/DRESSI.md"
test -f "$FRAG"
test -f "$ROOT/examples/demo_game/mod/demo_dressi.cfg"

echo "test_dressi.sh: ok"
