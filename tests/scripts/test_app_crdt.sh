#!/usr/bin/env bash
# App CRDT distributed Lua update wiring checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

APP_CRDT="$(idtech3_file engine/core/app_crdt.c src/qcommon/app_crdt.c)"
SV_CRDT="$(idtech3_file runtime/server/services/sv_app_crdt.c src/server/sv_app_crdt.c)"
CL_CRDT="$(idtech3_file runtime/client/core/cl_app_crdt.c src/client/core/cl_app_crdt.c)"
LUA_DBG="$(idtech3_file engine/core/lua_debug.c src/qcommon/lua_debug.c)"

grep -q 'AppCrdt_MergeLWW' "$APP_CRDT"
grep -q 'AppCrdt_QueueDispatch' "$APP_CRDT"
grep -q 'com_app_crdt' "$APP_CRDT"
grep -q 'SV_AppCrdt_Publish_f' "$SV_CRDT"
grep -q 'CL_AppCrdt_TryServerCommand' "$CL_CRDT"
grep -q 'on_app_crdt_message' "$LUA_DBG"
grep -q 'lua_setglobal( L, "AppCrdt" )' "$CL_CRDT"
grep -q 'SV_AppCrdt_OnMapReady' "$SV_CRDT"
grep -q 'AppCrdt_RefreshBackendRoot' "$APP_CRDT"
grep -q 'third_party/idtech3backend' "$ROOT/cmake/IdTech3Backend.cmake"
grep -q 'Engine.AppCrdt' "$SV_CRDT"
grep -q 'LuaDebug_SetScriptFallbackRoot' "$LUA_DBG"

grep -q 'idtech3_minimal_app_crdt_smoke' "$ROOT/tests/scripts/idtech3_minimal_content_smoke.sh"
if [[ "${IDTECH3_SKIP_RUNTIME_SMOKE:-0}" != "1" ]]; then
	echo "[test_app_crdt] minimal content runtime smoke..."
	"$ROOT/tests/scripts/idtech3_minimal_content_smoke.sh" app_crdt
fi

BACKEND_ROOT="$ROOT/third_party/idtech3backend"
[ -d "$BACKEND_ROOT" ] || BACKEND_ROOT="$ROOT/src/external/idtech3backend"
if [ ! -f "$BACKEND_ROOT/app_crdt/manifest.json" ]; then
	echo "SKIP: idtech3backend submodule not initialized (optional)"
	exit 0
fi
test -f "$BACKEND_ROOT/app_crdt/manifest.json"
test -f "$BACKEND_ROOT/server/lua/backend_app.lua"
test -f "$ROOT/docs/APP_CRDT.md"
test -f "$ROOT/examples/app_crdt/manifest.json"
test -f "$ROOT/tests/unit/test_app_crdt.c"

echo "test_app_crdt.sh: ok"
