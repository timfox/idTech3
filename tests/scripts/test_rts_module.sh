#!/usr/bin/env bash
# Guard RTS simulation module layout and ABI boundary.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

check_file() {
	[ -f "${ROOT}/$1" ] || fail "missing $1"
}

check_file modules/rts/rts_public.h
check_file modules/rts/rts_world.cpp
check_file modules/rts/rts_turn.cpp
check_file modules/rts/rts_command.cpp
check_file modules/rts/rts_entity.cpp
check_file modules/rts/rts_component.cpp
check_file modules/rts/rts_selection.cpp
check_file modules/rts/rts_obstruction.cpp
check_file modules/rts/rts_path_grid.cpp
check_file modules/rts/rts_vision.cpp
check_file modules/rts/rts_replay.cpp
check_file modules/rts/rts_usd_templates.cpp

rg -q 'extern "C"' "${ROOT}/modules/rts/rts_public.h" || fail "rts_public.h must expose a C-compatible API"
rg -q 'void RTS_Init\( void \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_Init missing"
rg -q 'void RTS_Shutdown\( void \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_Shutdown missing"
rg -q 'void RTS_RunTurn\( int msec \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_RunTurn missing"
rg -q 'int  RTS_PostCommand\( const rtsCommand_t \*cmd \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_PostCommand missing"
rg -q 'int  RTS_GetCurrentTurn\( void \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_GetCurrentTurn missing"
rg -q 'int  RTS_GetExecutedCommandCount\( void \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_GetExecutedCommandCount missing"
rg -q 'int  RTS_SelectRect\( int playerId' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_SelectRect missing"
rg -q 'unsigned RTS_ComputeStateHash\( void \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_ComputeStateHash missing"
rg -q 'std::stable_sort' "${ROOT}/modules/rts/rts_turn.cpp" || fail "turn command application must sort deterministically"
rg -q 'ADD_LIBRARY\(rts_module STATIC' "${ROOT}/CMakeLists.txt" || fail "rts_module CMake target missing"
rg -q 'modules/rts/rts_world.cpp' "${ROOT}/CMakeLists.txt" || fail "rts_world.cpp not wired"

echo "PASS: test_rts_module"
