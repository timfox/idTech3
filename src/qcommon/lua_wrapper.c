/*
===========================================================================
Copyright (C) 2024 id Tech 3

This file provides Lua integration for scripting support.
It wraps Lua functions with engine-style APIs.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_LUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// CVar to control Lua usage
static cvar_t *com_lua_enabled;

// Lua state pool
#define MAX_LUA_STATES 16
static lua_State *lua_states[MAX_LUA_STATES];
static int num_lua_states = 0;

/*
=================
Lua_Init
=================
Initialize Lua subsystem
=================
*/
void Lua_Init(void)
{
	int i;
	
	com_lua_enabled = Cvar_Get("com_lua_enabled", "1", CVAR_ARCHIVE | CVAR_LATCH);
	Cvar_SetDescription(com_lua_enabled, "Enable Lua scripting support (1 = enabled, 0 = disabled)");
	
	// Initialize Lua state pool
	for (i = 0; i < MAX_LUA_STATES; i++) {
		lua_states[i] = NULL;
	}
	num_lua_states = 0;
}

/*
=================
Lua_Shutdown
=================
Shutdown Lua subsystem
=================
*/
void Lua_Shutdown(void)
{
	int i;
	
	if (!com_lua_enabled || !com_lua_enabled->integer)
		return;
	
	// Close all Lua states
	for (i = 0; i < num_lua_states; i++) {
		if (lua_states[i]) {
			lua_close(lua_states[i]);
			lua_states[i] = NULL;
		}
	}
	num_lua_states = 0;
}

/*
=================
Lua_CreateState
=================
Create a new Lua state
Returns Lua state handle or NULL on failure
=================
*/
lua_State *Lua_CreateState(void)
{
	lua_State *L;
	
	if (!com_lua_enabled || !com_lua_enabled->integer)
		return NULL;
	
	if (num_lua_states >= MAX_LUA_STATES) {
		Com_Printf("Lua_CreateState: Maximum number of Lua states reached\n");
		return NULL;
	}
	
	L = luaL_newstate();
	if (!L)
		return NULL;
	
	// Open standard libraries
	luaL_openlibs(L);
	
	lua_states[num_lua_states++] = L;
	return L;
}

/*
=================
Lua_DestroyState
=================
Destroy a Lua state
=================
*/
void Lua_DestroyState(lua_State *L)
{
	int i;
	
	if (!L)
		return;
	
	// Find and remove from pool
	for (i = 0; i < num_lua_states; i++) {
		if (lua_states[i] == L) {
			lua_close(L);
			// Shift remaining states
			for (; i < num_lua_states - 1; i++) {
				lua_states[i] = lua_states[i + 1];
			}
			lua_states[num_lua_states - 1] = NULL;
			num_lua_states--;
			return;
		}
	}
	
	// Not in pool, close anyway
	lua_close(L);
}

/*
=================
Lua_LoadFile
=================
Load and execute a Lua file
Returns qtrue on success, qfalse on failure
=================
*/
qboolean Lua_LoadFile(lua_State *L, const char *filename)
{
	int result;
	
	if (!L || !filename || !*filename)
		return qfalse;
	
	if (!com_lua_enabled || !com_lua_enabled->integer)
		return qfalse;
	
	result = luaL_loadfile(L, filename);
	if (result != LUA_OK) {
		const char *error = lua_tostring(L, -1);
		Com_Printf("Lua_LoadFile: Error loading %s: %s\n", filename, error ? error : "Unknown error");
		lua_pop(L, 1);
		return qfalse;
	}
	
	result = lua_pcall(L, 0, LUA_MULTRET, 0);
	if (result != LUA_OK) {
		const char *error = lua_tostring(L, -1);
		Com_Printf("Lua_LoadFile: Error executing %s: %s\n", filename, error ? error : "Unknown error");
		lua_pop(L, 1);
		return qfalse;
	}
	
	return qtrue;
}

/*
=================
Lua_LoadString
=================
Load and execute a Lua string
Returns qtrue on success, qfalse on failure
=================
*/
qboolean Lua_LoadString(lua_State *L, const char *code)
{
	int result;
	
	if (!L || !code || !*code)
		return qfalse;
	
	if (!com_lua_enabled || !com_lua_enabled->integer)
		return qfalse;
	
	result = luaL_loadstring(L, code);
	if (result != LUA_OK) {
		const char *error = lua_tostring(L, -1);
		Com_Printf("Lua_LoadString: Error loading code: %s\n", error ? error : "Unknown error");
		lua_pop(L, 1);
		return qfalse;
	}
	
	result = lua_pcall(L, 0, LUA_MULTRET, 0);
	if (result != LUA_OK) {
		const char *error = lua_tostring(L, -1);
		Com_Printf("Lua_LoadString: Error executing code: %s\n", error ? error : "Unknown error");
		lua_pop(L, 1);
		return qfalse;
	}
	
	return qtrue;
}

/*
=================
Lua_CallFunction
=================
Call a Lua function by name
Returns qtrue on success, qfalse on failure
=================
*/
qboolean Lua_CallFunction(lua_State *L, const char *functionName, int numArgs, int numReturns)
{
	int result;
	
	if (!L || !functionName || !*functionName)
		return qfalse;
	
	if (!com_lua_enabled || !com_lua_enabled->integer)
		return qfalse;
	
	// Push function onto stack
	lua_getglobal(L, functionName);
	if (!lua_isfunction(L, -1)) {
		Com_Printf("Lua_CallFunction: Function '%s' not found\n", functionName);
		lua_pop(L, 1);
		return qfalse;
	}
	
	// Move function below arguments
	if (numArgs > 0) {
		lua_insert(L, -(numArgs + 1));
	}
	
	// Call function
	result = lua_pcall(L, numArgs, numReturns, 0);
	if (result != LUA_OK) {
		const char *error = lua_tostring(L, -1);
		Com_Printf("Lua_CallFunction: Error calling '%s': %s\n", functionName, error ? error : "Unknown error");
		lua_pop(L, 1);
		return qfalse;
	}
	
	return qtrue;
}

/*
=================
Lua_PushNumber
=================
Push a number onto the Lua stack
=================
*/
void Lua_PushNumber(lua_State *L, double n)
{
	if (L)
		lua_pushnumber(L, n);
}

/*
=================
Lua_PushString
=================
Push a string onto the Lua stack
=================
*/
void Lua_PushString(lua_State *L, const char *s)
{
	if (L && s)
		lua_pushstring(L, s);
}

/*
=================
Lua_PushBoolean
=================
Push a boolean onto the Lua stack
=================
*/
void Lua_PushBoolean(lua_State *L, qboolean b)
{
	if (L)
		lua_pushboolean(L, b ? 1 : 0);
}

/*
=================
Lua_GetNumber
=================
Get a number from the Lua stack
Returns 0.0 if invalid
=================
*/
double Lua_GetNumber(lua_State *L, int index)
{
	if (!L)
		return 0.0;
	
	if (lua_isnumber(L, index))
		return lua_tonumber(L, index);
	
	return 0.0;
}

/*
=================
Lua_GetString
=================
Get a string from the Lua stack
Returns NULL if invalid
=================
*/
const char *Lua_GetString(lua_State *L, int index)
{
	if (!L)
		return NULL;
	
	if (lua_isstring(L, index))
		return lua_tostring(L, index);
	
	return NULL;
}

/*
=================
Lua_GetBoolean
=================
Get a boolean from the Lua stack
Returns qfalse if invalid
=================
*/
qboolean Lua_GetBoolean(lua_State *L, int index)
{
	if (!L)
		return qfalse;
	
	if (lua_isboolean(L, index))
		return lua_toboolean(L, index) ? qtrue : qfalse;
	
	return qfalse;
}

/*
=================
Lua_RegisterFunction
=================
Register a C function with Lua
=================
*/
void Lua_RegisterFunction(lua_State *L, const char *name, lua_CFunction func)
{
	if (L && name && func)
		lua_register(L, name, func);
}

/*
=================
Lua_SetGlobal
=================
Set a global variable in Lua
=================
*/
void Lua_SetGlobal(lua_State *L, const char *name)
{
	if (L && name)
		lua_setglobal(L, name);
}

/*
=================
Lua_GetGlobal
=================
Get a global variable from Lua
=================
*/
void Lua_GetGlobal(lua_State *L, const char *name)
{
	if (L && name)
		lua_getglobal(L, name);
}

/*
=================
Lua_GetTop
=================
Get the number of elements on the Lua stack
=================
*/
int Lua_GetTop(lua_State *L)
{
	if (!L)
		return 0;
	
	return lua_gettop(L);
}

/*
=================
Lua_SetTop
=================
Set the number of elements on the Lua stack
=================
*/
void Lua_SetTop(lua_State *L, int index)
{
	if (L)
		lua_settop(L, index);
}

/*
=================
Lua_Pop
=================
Pop n elements from the Lua stack
=================
*/
void Lua_Pop(lua_State *L, int n)
{
	if (L && n > 0)
		lua_pop(L, n);
}

#endif // USE_LUA

