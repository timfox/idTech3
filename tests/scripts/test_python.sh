#!/usr/bin/env bash
# Python / Infernux scripting checks — arXiv:2604.10263
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

PYTHON_DEBUG="$(idtech3_file engine/core/python_debug.c src/qcommon/python_debug.c)"
PYTHON_BATCH="$(idtech3_file engine/core/python_batch.c src/qcommon/python_batch.c)"
CMD_C="$(idtech3_file engine/core/cmd.c src/qcommon/cmd.c)"
COMMON_C="$(idtech3_file engine/core/common.c src/qcommon/common.c)"

grep -q 'PyDebug_EnsureRuntime' "$PYTHON_DEBUG"
grep -q 'PyBatch_Read' "$PYTHON_BATCH"
grep -q 'py_reload' "$CMD_C"
grep -q 'Infernux_ConsoleInit' "$COMMON_C"
grep -q 'USE_PYTHON' "$ROOT/CMakeLists.txt"
test -f "$ROOT/docs/PYTHON.md"
test -f "$ROOT/scripts/python/idtech3/engine.py"
test -f "$ROOT/tests/unit/test_infernux_model.c"

echo "test_python.sh: ok"
