#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'Mimir_Benchmark' "$ROOT/src/extensions/research/mimir/mimir_model.c"
grep -q 'mimir_step' "$ROOT/src/renderers/vulkan/extensions/scaffold/vk_mimir.c"
grep -q 'mimir_brownian.comp' "$ROOT/scripts/compile_shaders.sh"
grep -q 'Mimir_ConsoleInit' "$ROOT/src/qcommon/common.c"
grep -q 'R_Mimir_Init' "$ROOT/src/renderers/vulkan/tr_init.c"
test -f "$ROOT/docs/MIMIR.md"
test -f "$ROOT/tests/unit/test_mimir_model.c"

echo "test_mimir.sh: ok"
