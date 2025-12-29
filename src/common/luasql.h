/*
===========================================================================
id Tech 3 - LuaSQL Interface Header

LuaSQL integration for database access from Lua scripts.
Based on LuaSQL implementation.
===========================================================================
*/

#ifdef USE_LUASQL

#ifndef INCLUDE_LUASQL_H
#define INCLUDE_LUASQL_H

#ifdef USE_LUA

#include <lua.h>
#include <lauxlib.h>

#define LUASQL_PREFIX "LuaSQL: "
#define LUASQL_TABLENAME "luasql"
#define LUASQL_ENVIRONMENT "Each driver must have an environment metatable"
#define LUASQL_CONNECTION "Each driver must have a connection metatable"
#define LUASQL_CURSOR "Each driver must have a cursor metatable"

#define LUASQL_ENVIRONMENT_SQLITE "SQLite3 environment"
#define LUASQL_CONNECTION_SQLITE "SQLite3 connection"
#define LUASQL_CURSOR_SQLITE "SQLite3 cursor"

// LuaSQL API functions
int luasql_faildirect(lua_State *L, const char *err);
int luasql_failmsg(lua_State *L, const char *err, const char *m);
int luasql_createmeta(lua_State *L, const char *name, const luaL_Reg *methods);
void luasql_setmeta(lua_State *L, const char *name);
void luasql_set_info(lua_State *L);

// Driver initialization
int luaopen_luasql_sqlite3(lua_State *L);

// Lua compatibility
#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM == 501
void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup);
#endif

#endif // USE_LUA

#endif // INCLUDE_LUASQL_H

#endif // USE_LUASQL