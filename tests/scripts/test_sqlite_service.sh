#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

ENGINE_DB="$(idtech3_file engine/core/engine_db.c src/qcommon/engine_db.c)"
CSHARP_ENGINE="$(idtech3_file engine/core/csharp/IdTech3.Engine.cs src/qcommon/csharp/IdTech3.Engine.cs)"
PYTHON_DEBUG="$(idtech3_file engine/core/python_debug.c src/qcommon/python_debug.c)"
LUA_BINDINGS="$(idtech3_file runtime/game/g_lua_bindings.c src/game/g_lua_bindings.c)"
LUA_REGISTRATION="$(idtech3_file runtime/game/g_lua_registration.inc src/game/g_lua_registration.inc)"

grep -q 'option(USE_SQLITE "Enable native SQLite gameplay/profile database support" ON)' "$ROOT/CMakeLists.txt" || fail "missing USE_SQLITE option"
grep -q 'find_package(SQLite3 QUIET)' "$ROOT/CMakeLists.txt" || fail "missing SQLite3 discovery"
grep -q 'EngineDB_ProfileSet' "$ENGINE_DB" || fail "missing EngineDB profile store"
grep -q 'EngineDB_SaveWriteSlot' "$ENGINE_DB" || fail "missing EngineDB save slot store"
grep -q 'registerTable(L, "DB", dbFuncs);' "$LUA_BINDINGS" "$LUA_REGISTRATION" || fail "missing Lua Engine.DB table"
grep -q 'db_query_one' "$PYTHON_DEBUG" || fail "missing Python db query hook"
grep -q 'DbQueryOne' "$CSHARP_ENGINE" || fail "missing C# db query API"
grep -q 'Engine.DB' "$ROOT/docs/LUA_API.md" || fail "missing Lua DB docs"

echo "OK: SQLite service wiring present"
