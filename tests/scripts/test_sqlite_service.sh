#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

grep -q 'option(USE_SQLITE "Enable native SQLite gameplay/profile database support" ON)' "$ROOT/CMakeLists.txt" || fail "missing USE_SQLITE option"
grep -q 'find_package(SQLite3 QUIET)' "$ROOT/CMakeLists.txt" || fail "missing SQLite3 discovery"
grep -q 'EngineDB_ProfileSet' "$ROOT/src/qcommon/engine_db.c" || fail "missing EngineDB profile store"
grep -q 'EngineDB_SaveWriteSlot' "$ROOT/src/qcommon/engine_db.c" || fail "missing EngineDB save slot store"
grep -q 'registerTable(L, "DB", dbFuncs);' "$ROOT/runtime/game/g_lua_bindings.c" || fail "missing Lua Engine.DB table"
grep -q 'db_query_one' "$ROOT/engine/core/python_debug.c" || fail "missing Python db query hook"
grep -q 'DbQueryOne' "$ROOT/src/qcommon/csharp/IdTech3.Engine.cs" || fail "missing C# db query API"
grep -q 'Engine.DB' "$ROOT/docs/LUA_API.md" || fail "missing Lua DB docs"

echo "OK: SQLite service wiring present"
