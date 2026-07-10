#!/usr/bin/env bash
# Tree Traversal Prefetcher (TTP) model checks — arXiv:2605.16253
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

TTP_MODEL="$(idtech3_file engine/core/ttp_model.c src/qcommon/ttp_model.c)"
TTP_SIM="$(idtech3_file engine/core/ttp_sim.c src/qcommon/ttp_sim.c)"
TTP_C="$(idtech3_file engine/core/ttp.c src/qcommon/ttp.c)"
COMMON_C="$(idtech3_file engine/core/common.c src/qcommon/common.c)"

grep -q 'TTP_ModelDFS' "$TTP_MODEL"
grep -q 'TTP_BFSCoverageForDistance' "$TTP_MODEL"
grep -q 'TTP_SimulateTraversal' "$TTP_SIM"
grep -q 'ttp_lumibench' "$TTP_C"
grep -q 'ttp_bfs_sweep' "$TTP_C"
grep -q 'TTP_Init' "$COMMON_C"
grep -q 'Tree Traversal Prefetcher' "$ROOT/docs/TTP.md"
grep -q '98.92%' "$ROOT/docs/TTP.md"
test -f "$ROOT/tests/unit/test_ttp_model.c"

echo "test_ttp.sh: ok"
