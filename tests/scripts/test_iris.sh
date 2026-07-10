#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

IRIS_MODEL="$(idtech3_file extensions/research/iris/iris_model.c src/extensions/research/iris/iris_model.c)"
IRIS_IO="$(idtech3_file extensions/research/iris/iris_io.c src/extensions/research/iris/iris_io.c)"
VK_IRIS="$(idtech3_file renderers/vulkan/extensions/scaffold/vk_iris.c src/renderers/vulkan/extensions/scaffold/vk_iris.c)"
IRIS_COMPOSE="$(idtech3_file renderers/vulkan/shaders/glsl/iris/iris_compose.comp src/renderers/vulkan/shaders/glsl/iris/iris_compose.comp)"
COMMON="$(idtech3_file engine/core/common.c src/qcommon/common.c)"
TR_INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"

grep -q 'Iris_ModelBenchmark' "$IRIS_MODEL"
grep -q 'Iris_SerializeAtlas' "$IRIS_IO"
grep -q 'iris_load' "$VK_IRIS"
grep -q 'mipBilinear' "$IRIS_COMPOSE"
grep -q 'Iris_DispatchSpdForHrTiles' "$VK_IRIS"
grep -q 'iris_overlay.comp' "$ROOT/scripts/compile_shaders.sh"
grep -q 'r_iris_bilinear' "$VK_IRIS"
grep -q 'Iris_ConsoleInit' "$COMMON"
grep -q 'R_Iris_Init' "$TR_INIT"
test -f "$ROOT/docs/IRIS.md"
test -f "$ROOT/tests/unit/test_iris_model.c"
test -f "$ROOT/tests/unit/test_iris_io.c"

echo "test_iris.sh: ok"
