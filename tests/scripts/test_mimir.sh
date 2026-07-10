#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

MIMIR_MODEL="$(idtech3_file extensions/research/mimir/mimir_model.c src/extensions/research/mimir/mimir_model.c)"
VK_MIMIR="$(idtech3_file renderers/vulkan/extensions/scaffold/vk_mimir.c src/renderers/vulkan/extensions/scaffold/vk_mimir.c)"
COMMON="$(idtech3_file engine/core/common.c src/qcommon/common.c)"
TR_INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"

grep -q 'Mimir_Benchmark' "$MIMIR_MODEL"
grep -q 'mimir_step' "$VK_MIMIR"
grep -q 'mimir_display' "$VK_MIMIR"
grep -q 'mimir_display_async' "$VK_MIMIR"
grep -q 'mimir_brownian.comp' "$ROOT/scripts/compile_shaders.sh"
grep -q 'Mimir_ConsoleInit' "$COMMON"
grep -q 'R_Mimir_Init' "$TR_INIT"
test -f "$ROOT/docs/MIMIR.md"
test -f "$ROOT/tests/unit/test_mimir_model.c"

echo "test_mimir.sh: ok"
