#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'Iris_ModelBenchmark' "$ROOT/src/extensions/research/iris/iris_model.c"
grep -q 'Iris_SerializeAtlas' "$ROOT/src/extensions/research/iris/iris_io.c"
grep -q 'iris_load' "$ROOT/src/renderers/vulkan/vk_iris.c"
grep -q 'mipBilinear' "$ROOT/src/renderers/vulkan/shaders/glsl/iris/iris_compose.comp"
grep -q 'Iris_DispatchSpdForHrTiles' "$ROOT/src/renderers/vulkan/vk_iris.c"
grep -q 'iris_overlay.comp' "$ROOT/scripts/compile_shaders.sh"
grep -q 'r_iris_bilinear' "$ROOT/src/renderers/vulkan/vk_iris.c"
grep -q 'Iris_ConsoleInit' "$ROOT/src/qcommon/common.c"
grep -q 'R_Iris_Init' "$ROOT/src/renderers/vulkan/tr_init.c"
test -f "$ROOT/docs/IRIS.md"
test -f "$ROOT/tests/unit/test_iris_model.c"
test -f "$ROOT/tests/unit/test_iris_io.c"

echo "test_iris.sh: ok"
