#!/usr/bin/env bash
# Static guards for optional C# scripting (Mono).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$SCRIPT_DIR/idtech3_test_paths.sh"
idtech3_test_paths_init "$PROJECT_ROOT"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

CS_DEBUG="$(idtech3_file engine/core/csharp_debug.c src/qcommon/csharp_debug.c)"
CMD="$(idtech3_file engine/core/cmd.c src/qcommon/cmd.c)"
SCRIPT_EMIT="$(idtech3_file engine/core/script_emit.c src/qcommon/script_emit.c)"
COMMON="$(idtech3_file engine/core/common.c src/qcommon/common.c)"
CL_MAIN="$(idtech3_file runtime/client/core/cl_main.c src/client/core/cl_main.c)"
CS_API="$(idtech3_file engine/core/csharp/IdTech3.Engine.cs src/qcommon/csharp/IdTech3.Engine.cs)"

grep -q 'option(USE_CSHARP' "$PROJECT_ROOT/CMakeLists.txt" || fail "USE_CSHARP CMake option missing"
[ -f "$CS_DEBUG" ] || fail "csharp_debug.c missing"
grep -q 'Cmd_CsReload_f' "$CMD" || fail "cs_reload not registered in cmd.c"
grep -q 'Com_ScriptEmitEvent' "$SCRIPT_EMIT" || fail "script_emit bridge missing"
grep -q 'CsDebug_Frame' "$COMMON" || fail "CsDebug_Frame not called from Com_Frame"
grep -q 'cs_allowExec' "$CS_DEBUG" || fail "cs_allowExec cvar missing"
grep -q 'LuaDebug_SetEngineRegisterCallback' "$CL_MAIN" || fail "client must register Lua Engine.* callback"
[ -f "$CS_API" ] || fail "IdTech3.Engine.cs missing"
[ -f "$PROJECT_ROOT/docs/CSHARP.md" ] || fail "docs/CSHARP.md missing"

echo "PASS: C# scripting scaffolding"
