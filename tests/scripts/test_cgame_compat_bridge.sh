#!/usr/bin/env bash
# Wiring test: keep retail-QVM ABI glue behind dedicated cgame bridge helpers.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FILE="$ROOT/runtime/client/core/cl_cgame.c"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

grep -q 'CL_UsesLegacyQvmLayout' "$FILE" || fail 'missing legacy layout gate helper'
grep -q 'CL_WriteTraceResult' "$FILE" || fail 'missing trace export helper'
grep -q 'CL_AddRefEntityToSceneFromCgame' "$FILE" || fail 'missing ref entity bridge helper'
grep -q 'CL_ExportGameStateToCgame' "$FILE" || fail 'missing gameState bridge helper'
grep -q 'CL_ExportSnapshotToCgame' "$FILE" || fail 'missing snapshot bridge helper'

grep -q 'CL_WriteTraceResult( 1, VMA(1), &trace )' "$FILE" || fail 'trace syscalls must use trace export helper'
grep -q 'CL_AddRefEntityToSceneFromCgame( 1, VMA(1), qfalse )' "$FILE" || fail 'CG_R_ADDREFENTITYTOSCENE must use bridge helper'
grep -q 'CL_AddRefEntityToSceneFromCgame( 1, VMA(1), qtrue )' "$FILE" || fail 'CG_R_ADDREFENTITYTOSCENE2 must use bridge helper'
grep -q 'CL_ExportGameStateToCgame( 1, VMA(1) )' "$FILE" || fail 'CG_GETGAMESTATE must use bridge helper'
grep -q 'CL_ExportSnapshotToCgame( args\[1\], 2, VMA(2) )' "$FILE" || fail 'CG_GETSNAPSHOT must use bridge helper'

pass "cgame compatibility bridge helpers are wired"
