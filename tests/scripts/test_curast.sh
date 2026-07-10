#!/usr/bin/env bash
# CuRast software rasterization checks — arXiv:2604.21749
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

CURAST_MODEL="$(idtech3_file extensions/research/curast/curast_model.c src/extensions/research/curast/curast_model.c)"
VK_CURAST="$(idtech3_file renderers/vulkan/extensions/scaffold/vk_curast.c src/renderers/vulkan/extensions/scaffold/vk_curast.c)"
COMMON="$(idtech3_file engine/core/common.c src/qcommon/common.c)"

grep -q 'CuRast_ModelBenchmark' "$CURAST_MODEL"
grep -q 'curast_render' "$VK_CURAST"
grep -q 'curast_partition' "$VK_CURAST"
grep -q 'CuRast_AnalyzeStageRouting' "$VK_CURAST"
grep -q 'curast_stage1.comp' "$ROOT/scripts/compile_shaders.sh"
grep -q 'Curast_ConsoleInit' "$COMMON"
test -f "$ROOT/docs/CURAST.md"
test -f "$ROOT/tests/unit/test_curast_model.c"

echo "test_curast.sh: ok"
