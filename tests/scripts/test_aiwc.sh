#!/usr/bin/env bash
# AIWC architecture-independent memory characterization checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'AIWC_EntropyBitsDropped' "$ROOT/src/qcommon/aiwc_metrics.c"
grep -q 'AIWC_SimulateMatmul' "$ROOT/src/qcommon/aiwc_matmul.c"
grep -q 'aiwc_matmul_all' "$ROOT/src/qcommon/aiwc.c"
grep -q 'AIWC_Init' "$ROOT/src/qcommon/common.c"
grep -q 'parallel spatial locality' "$ROOT/docs/AIWC.md"
test -f "$ROOT/tests/unit/test_aiwc_metrics.c"

echo "test_aiwc.sh: ok"
