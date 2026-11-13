#!/usr/bin/env bash
# Static compatibility and attribution gates; no game data or display required.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TV="$ROOT/runtime/client/core/cl_tv.c"
DOC="$ROOT/docs/TRINITY_COMPATIBILITY.md"

fail() { echo "test_trinity_compatibility: $*" >&2; exit 1; }
check() { grep -q "$2" "$1" || fail "$3"; }

check "$TV" 'Cvar_Get( "cl_tvSkip", "10"' "missing cl_tvSkip"
check "$TV" 'Cmd_AddCommand( "tv_forward"' "missing tv_forward"
check "$TV" 'Cmd_AddCommand( "tv_backward"' "missing tv_backward"
check "$TV" 'Cmd_AddCommand( "tv_next"' "missing tv_next"
check "$TV" 'Cmd_AddCommand( "tv_prev"' "missing tv_prev"
check "$TV" 'Cannot seek a live TV stream' "live streams do not reject seeks"
check "$TV" "Trinity's game module" "implementation-site Trinity credit missing"
check "$DOC" 'github.com/ernie/trinity-engine' "Trinity Engine credit missing"
check "$DOC" 'github.com/ernie/trinity)' "Trinity game credit missing"
check "$DOC" 'github.com/ernie/trinity-vr' "Trinity VR credit missing"

echo "test_trinity_compatibility: PASS"
