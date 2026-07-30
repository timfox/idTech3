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
check_file modules/rts/rts_gui.cpp
check_file modules/rts/rts_obstruction.cpp
check_file modules/rts/rts_path_grid.cpp
check_file modules/rts/rts_vision.cpp
check_file modules/rts/rts_replay.cpp
check_file modules/rts/rts_usd_templates.cpp
check_file docs/RTS_MODEL_RENDERING.md
check_file docs/RTS_GUI_PORT.md
check_file runtime/client/core/cl_rts_gui.c
check_file scripts/js/rts_hud.js
check_file config/demo_rts_0ad.cfg

rg -q 'extern "C"' "${ROOT}/modules/rts/rts_public.h" || fail "rts_public.h must expose a C-compatible API"
rg -q 'void RTS_Init\( void \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_Init missing"
rg -q 'void RTS_Shutdown\( void \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_Shutdown missing"
rg -q 'void RTS_RunTurn\( int msec \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_RunTurn missing"
rg -q 'int  RTS_PostCommand\( const rtsCommand_t \*cmd \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_PostCommand missing"
rg -q 'int  RTS_GetCurrentTurn\( void \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_GetCurrentTurn missing"
rg -q 'int  RTS_GetExecutedCommandCount\( void \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_GetExecutedCommandCount missing"
rg -q 'int  RTS_SelectRect\( int playerId' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_SelectRect missing"
rg -q 'RTS_SetEntityModel' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_SetEntityModel missing"
rg -q 'RTS_SetDefaultModelForOwner' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_SetDefaultModelForOwner missing"
rg -q 'RTS_BuildRenderEntities' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_BuildRenderEntities missing"
rg -q 'RTS_GuiGetState' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_GuiGetState missing"
rg -q 'RTS_GuiIssueMoveSelected' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_GuiIssueMoveSelected missing"
rg -q 'rtsRenderEntity_t' "${ROOT}/modules/rts/rts_public.h" || fail "rtsRenderEntity_t missing"
rg -q 'unsigned RTS_ComputeStateHash\( void \);' "${ROOT}/modules/rts/rts_public.h" || fail "RTS_ComputeStateHash missing"
rg -q 'std::stable_sort' "${ROOT}/modules/rts/rts_turn.cpp" || fail "turn command application must sort deterministically"
rg -q 'modelHandle' "${ROOT}/modules/rts/rts_internal.h" || fail "RTS entities must store renderer model handles"
rg -q 'modelPath' "${ROOT}/modules/rts/rts_internal.h" || fail "RTS entities must store model paths"
rg -q 'RE_RegisterModel' "${ROOT}/docs/RTS_MODEL_RENDERING.md" || fail "RTS model rendering doc must describe renderer registration"
rg -q 'RTS_BuildRenderEntities' "${ROOT}/docs/RTS_MODEL_RENDERING.md" || fail "RTS model rendering doc must describe render export"
rg -q 'ADD_LIBRARY\(rts_module STATIC' "${ROOT}/CMakeLists.txt" || fail "rts_module CMake target missing"
rg -q 'modules/rts/rts_world.cpp' "${ROOT}/CMakeLists.txt" || fail "rts_world.cpp not wired"
rg -q 'modules/rts/rts_gui.cpp' "${ROOT}/CMakeLists.txt" || fail "rts_gui.cpp not wired"
rg -q 'idtech3.rts' "${ROOT}/docs/RTS_GUI_PORT.md" || fail "RTS GUI doc must describe JavaScript API"
rg -q 'idtech3.rts' "${ROOT}/scripts/js/rts_hud.js" || fail "RTS JS HUD must use idtech3.rts"
rg -q 'js_reload scripts/js/rts_hud.js' "${ROOT}/config/demo_rts_0ad.cfg" || fail "RTS demo config must load the JS HUD"
rg -q 'Js_Binding_RtsState' "${ROOT}/engine/core/js_debug.c" || fail "JavaScript RTS state binding missing"
! rg -q 'CL_RTSGui_RegisterLua' "${ROOT}/runtime/client/core" || fail "RTS GUI must not register Lua bindings"
! rg -q 'Engine.RTS' "${ROOT}/docs/RTS_GUI_PORT.md" || fail "RTS GUI docs must not point users at Lua"

echo "PASS: test_rts_module"
