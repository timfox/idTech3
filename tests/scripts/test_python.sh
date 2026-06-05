#!/usr/bin/env bash
# Python / Infernux scripting checks — arXiv:2604.10263
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'PyDebug_EnsureRuntime' "$ROOT/src/qcommon/python_debug.c"
grep -q 'PyBatch_Read' "$ROOT/src/qcommon/python_batch.c"
grep -q 'py_reload' "$ROOT/src/qcommon/cmd.c"
grep -q 'Infernux_ConsoleInit' "$ROOT/src/qcommon/common.c"
grep -q 'USE_PYTHON' "$ROOT/CMakeLists.txt"
test -f "$ROOT/docs/PYTHON.md"
test -f "$ROOT/scripts/python/idtech3/engine.py"
test -f "$ROOT/tests/unit/test_infernux_model.c"

echo "test_python.sh: ok"
