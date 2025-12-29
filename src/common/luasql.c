/*
===========================================================================
id Tech 3 - LuaSQL Implementation

LuaSQL core functions for database integration.
Based on LuaSQL library.
===========================================================================
*/

#ifdef USE_LUASQL

#include "luasql.h"
#include <stdio.h>

#ifdef USE_LUA

/*
===============
luasql_faildirect

Push error message directly to Lua stack
===============
*/
int luasql_faildirect(lua_State *L, const char *err)
{
	lua_pushnil(L);
	lua_pushstring(L, err);
	return 2;
}

/*
===============
luasql_failmsg

Push formatted error message to Lua stack
===============
*/
int luasql_failmsg(lua_State *L, const char *err, const char *m)
{
	char msg[256];
	sprintf(msg, "%s: %s", err, m ? m : "unknown error");
	return luasql_faildirect(L, msg);
}

/*
===============
luasql_createmeta

Create a metatable for LuaSQL objects
===============
*/
int luasql_createmeta(lua_State *L, const char *name, const luaL_Reg *methods)
{
	if (!luaL_newmetatable(L, name)) {
		return 0;
	}

	// Set methods
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);
	lua_settable(L, -3);

#if defined LUA_VERSION_NUM && LUA_VERSION_NUM >= 502
	luaL_setfuncs(L, methods, 0);
#else
	luaL_register(L, NULL, methods);
#endif

	return 1;
}

/*
===============
luasql_setmeta

Set metatable for userdata
===============
*/
void luasql_setmeta(lua_State *L, const char *name)
{
	luaL_getmetatable(L, name);
	lua_setmetatable(L, -2);
}

/*
===============
luasql_set_info

Set version info in LuaSQL table
===============
*/
void luasql_set_info(lua_State *L)
{
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (C) 2003-2012 Kepler Project");
	lua_settable(L, -3);

	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "LuaSQL is a simple interface from Lua to a DBMS");
	lua_settable(L, -3);

	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "LuaSQL 2.3.0");
	lua_settable(L, -3);
}

// luaopen_luasql_sqlite3 is implemented in ls_sqlite3.c

#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM == 501
/*
===============
luaL_setfuncs (Lua 5.1 compatibility)

Set functions in table at top of stack
===============
*/
void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup)
{
	for (; l->name; l++) {
		int i;
		for (i = 0; i < nup; i++) {
			lua_pushvalue(L, -nup);
		}
		lua_pushcclosure(L, l->func, nup);
		lua_setfield(L, -(nup + 2), l->name);
	}
	lua_pop(L, nup);
}
#endif

#endif // USE_LUA

#endif // USE_LUASQL