#!/usr/bin/env bash
# App CRDT distributed Lua update wiring checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'AppCrdt_MergeLWW' "$ROOT/src/qcommon/app_crdt.c"
grep -q 'AppCrdt_QueueDispatch' "$ROOT/src/qcommon/app_crdt.c"
grep -q 'com_app_crdt' "$ROOT/src/qcommon/app_crdt.c"
grep -q 'SV_AppCrdt_Publish_f' "$ROOT/src/server/sv_app_crdt.c"
grep -q 'CL_AppCrdt_TryServerCommand' "$ROOT/src/client/cl_app_crdt.c"
grep -q 'on_app_crdt_message' "$ROOT/src/qcommon/lua_debug.c"
grep -q 'lua_setglobal( L, "AppCrdt" )' "$ROOT/src/client/cl_app_crdt.c"
grep -q 'SV_AppCrdt_OnMapReady' "$ROOT/src/server/sv_app_crdt.c"
grep -q 'AppCrdt_RefreshBackendRoot' "$ROOT/src/qcommon/app_crdt.c"
grep -q 'Engine.AppCrdt' "$ROOT/src/server/sv_app_crdt.c"
grep -q 'LuaDebug_SetScriptFallbackRoot' "$ROOT/src/qcommon/lua_debug.c"
test -f "$ROOT/src/external/idtech3backend/app_crdt/manifest.json"
test -f "$ROOT/src/external/idtech3backend/server/lua/backend_app.lua"
test -f "$ROOT/docs/APP_CRDT.md"
test -f "$ROOT/examples/app_crdt/manifest.json"
test -f "$ROOT/tests/unit/test_app_crdt.c"

echo "test_app_crdt.sh: ok"
