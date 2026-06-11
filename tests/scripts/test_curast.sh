#!/usr/bin/env bash
# CuRast software rasterization checks — arXiv:2604.21749
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'CuRast_ModelBenchmark' "$ROOT/src/extensions/research/curast/curast_model.c"
grep -q 'curast_render' "$ROOT/src/renderers/vulkan/vk_curast.c"
grep -q 'curast_stage1.comp' "$ROOT/scripts/compile_shaders.sh"
grep -q 'Curast_ConsoleInit' "$ROOT/src/qcommon/common.c"
test -f "$ROOT/docs/CURAST.md"
test -f "$ROOT/tests/unit/test_curast_model.c"

echo "test_curast.sh: ok"
