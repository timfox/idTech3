#!/usr/bin/env bash
# Tree Traversal Prefetcher (TTP) model checks — arXiv:2605.16253
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'TTP_ModelDFS' "$ROOT/src/qcommon/ttp_model.c"
grep -q 'TTP_SimulateTraversal' "$ROOT/src/qcommon/ttp_sim.c"
grep -q 'ttp_lumibench' "$ROOT/src/qcommon/ttp.c"
grep -q 'TTP_Init' "$ROOT/src/qcommon/common.c"
grep -q 'Tree Traversal Prefetcher' "$ROOT/docs/TTP.md"
test -f "$ROOT/tests/unit/test_ttp_model.c"

echo "test_ttp.sh: ok"
