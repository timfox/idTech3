#!/usr/bin/env bash
# BubbleSH dataset compact-state checks — arXiv:2607.07275
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

BUBBLESH_MODEL="$(idtech3_file engine/core/bubblesh_model.c src/qcommon/bubblesh_model.c)"
BUBBLESH_C="$(idtech3_file engine/core/bubblesh.c src/qcommon/bubblesh.c)"
COMMON_C="$(idtech3_file engine/core/common.c src/qcommon/common.c)"

grep -q 'BubbleSH_CoefficientCount' "$BUBBLESH_MODEL"
grep -q 'BubbleSH_NormalizedWasserstein1' "$BUBBLESH_MODEL"
grep -q 'bubblesh_metrics' "$BUBBLESH_C"
grep -q 'BubbleSH_Init' "$COMMON_C"
grep -q 'BubbleSH' "$ROOT/docs/BUBBLESH.md"
grep -q 'R-ADE' "$ROOT/docs/BUBBLESH.md"
test -f "$ROOT/tests/unit/test_bubblesh_model.c"

echo "test_bubblesh.sh: ok"
