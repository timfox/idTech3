#!/usr/bin/env bash
# BubbleSH dataset compact-state checks — arXiv:2607.07275
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'BubbleSH_CoefficientCount' "$ROOT/src/qcommon/bubblesh_model.c"
grep -q 'BubbleSH_NormalizedWasserstein1' "$ROOT/src/qcommon/bubblesh_model.c"
grep -q 'bubblesh_metrics' "$ROOT/src/qcommon/bubblesh.c"
grep -q 'BubbleSH_Init' "$ROOT/src/qcommon/common.c"
grep -q 'BubbleSH' "$ROOT/docs/BUBBLESH.md"
grep -q 'R-ADE' "$ROOT/docs/BUBBLESH.md"
test -f "$ROOT/tests/unit/test_bubblesh_model.c"

echo "test_bubblesh.sh: ok"
