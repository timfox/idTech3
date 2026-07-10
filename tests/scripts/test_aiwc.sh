#!/usr/bin/env bash
# AIWC architecture-independent memory characterization checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

AIWC_METRICS="$(idtech3_file engine/core/aiwc_metrics.c src/qcommon/aiwc_metrics.c)"
AIWC_MATMUL="$(idtech3_file engine/core/aiwc_matmul.c src/qcommon/aiwc_matmul.c)"
AIWC_C="$(idtech3_file engine/core/aiwc.c src/qcommon/aiwc.c)"
COMMON_C="$(idtech3_file engine/core/common.c src/qcommon/common.c)"

grep -q 'AIWC_EntropyBitsDropped' "$AIWC_METRICS"
grep -q 'AIWC_SimulateMatmul' "$AIWC_MATMUL"
grep -q 'aiwc_matmul_all' "$AIWC_C"
grep -q 'AIWC_Init' "$COMMON_C"
grep -q 'parallel spatial locality' "$ROOT/docs/AIWC.md"
test -f "$ROOT/tests/unit/test_aiwc_metrics.c"

echo "test_aiwc.sh: ok"
