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
#include "lua_events.h"
#include "lua_coroutine.h"
#include "lua_entity.h"
#include "lua_encounter.h"
#include "lua_sequence.h"
// Forward declarations for module bindings
void Lua_RegisterGameBindings(lua_State *L);
void Lua_RegisterRendererBindings(lua_State *L);
void Lua_RegisterSoundBindings(lua_State *L);
void Lua_Events_RegisterBindings(lua_State *L);
void Lua_Entity_RegisterBindings(lua_State *L);

// fs_basepath and current game dir are queried via Cvar API to avoid
// direct linkage against internal file system globals.

// Forward declaration for internal function
static void Lua_LoadScriptsFromFS(lua_State *L);

// Forward declaration for console command
static void Lua_ReloadScript_f(void);

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
	
	// Register console command for hot reload
	Cmd_AddCommand("lua_reload", Lua_ReloadScript_f);
	
	// Initialize event bus
	Lua_Events_Init();
	
	// Initialize coroutine scheduler
	Lua_Coroutine_Init();
	
	// Initialize entity script system
	Lua_Entity_Init();
	
	// Initialize encounter system
	Lua_Encounter_Init();
	
	// Initialize sequence system
	Lua_Sequence_Init();
	
	// Initialize Lua state pool
	for (i = 0; i < MAX_LUA_STATES; i++) {
		lua_states[i] = NULL;
	}
	num_lua_states = 0;

	// Create the primary Lua state immediately so scripts and bindings
	// are ready for use once initialization completes.
	if (!Lua_CreateState()) {
		Com_Printf("Lua_Init: Failed to create main Lua state\n");
	}
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
	
	// Remove console command
	Cmd_RemoveCommand("lua_reload");
	
	// Shutdown sequence system
	Lua_Sequence_Shutdown();
	
	// Shutdown encounter system
	Lua_Encounter_Shutdown();
	
	// Shutdown entity script system
	Lua_Entity_Shutdown();
	
	// Shutdown coroutine scheduler
	Lua_Coroutine_Shutdown();
	
	// Shutdown event bus
	Lua_Events_Shutdown();
	
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

	// Prepend mod script paths so `require` can find scripts/lib/*.lua using absolute paths
	lua_getglobal(L, "package");
	if ( lua_istable(L, -1) ) {
		const char *existing = NULL;
		lua_getfield(L, -1, "path");
		if ( lua_isstring(L, -1) ) {
			existing = lua_tostring(L, -1);
		}
		lua_pop(L, 1); // pop existing path

		char newPath[4096];
		newPath[0] = '\0';
		// Prepend fs_basepath/fs_game if available (use Cvar API to avoid linking internal globals)
		char basepath[MAX_OSPATH] = {0};
		char gamedir[MAX_OSPATH] = {0};
		Cvar_VariableStringBuffer( "fs_basepath", basepath, sizeof(basepath) );
		Cvar_VariableStringBuffer( "fs_game", gamedir, sizeof(gamedir) );
		if ( basepath[0] && gamedir[0] ) {
			Com_sprintf( newPath, sizeof(newPath), "%s/%s/scripts/?.lua;%s/%s/scripts/lib/?.lua;",
				basepath, gamedir, basepath, gamedir );
		}
		// Always include local fallbacks
		Q_strcat( newPath, sizeof(newPath), "scripts/?.lua;scripts/lib/?.lua;./scripts/?.lua;./scripts/lib/?.lua;" );
		// Append existing search path
		if ( existing && existing[0] ) {
			Q_strcat( newPath, sizeof(newPath), existing );
		}
		lua_pushstring(L, newPath );
		lua_setfield(L, -2, "path");
	}
	lua_pop(L, 1); // pop package
	
	// Register engine bindings
	Lua_RegisterEngineBindings(L);
	
	// Register event bus bindings
	Lua_Events_RegisterBindings(L);
	
	// Register coroutine bindings
	Lua_Coroutine_RegisterBindings(L);
	
	// Register entity script bindings
	Lua_Entity_RegisterBindings(L);
	
	// Register encounter bindings
	Lua_Encounter_RegisterBindings(L);
	
	// Register sequence bindings
	Lua_Sequence_RegisterBindings(L);
	
	// Register module-specific bindings
	Lua_RegisterGameBindings(L);
	Lua_RegisterRendererBindings(L);
	Lua_RegisterSoundBindings(L);
	
	// Load scripts from filesystem
	Lua_LoadScriptsFromFS(L);
	
	lua_states[num_lua_states++] = L;
	return L;
}

// Lua_LoadScriptsFromFS
// Load all Lua scripts from filesystem (scripts/*.lua)
static void Lua_LoadScriptsFromFS(lua_State *L)
{
	char **fileList;
	int numFiles;
	int i;
	char filename[MAX_QPATH];
	
	if (!L || !com_lua_enabled || !com_lua_enabled->integer)
		return;
	
	// Find all .lua files in scripts directory
	fileList = FS_ListFiles("scripts", ".lua", &numFiles);
	if (!fileList || numFiles <= 0)
		return;
	
	for (i = 0; i < numFiles; i++) {
		if (!fileList[i])
			continue;
		
		Com_sprintf(filename, sizeof(filename), "scripts/%s", fileList[i]);
		Com_DPrintf("Loading Lua script: %s\n", filename);
		Lua_LoadScriptFromFS(L, filename);
	}
	
	FS_FreeFileList(fileList);
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

/*
=================
Lua_LoadScriptFromFS
=================
Load and execute a Lua script from filesystem
Searches in mod directories and pk3 files
=================
*/
qboolean Lua_LoadScriptFromFS(lua_State *L, const char *filename)
{
	fileHandle_t f;
	int len;
	char *buffer;
	
	if (!L || !filename || !*filename)
		return qfalse;
	
	if (!com_lua_enabled || !com_lua_enabled->integer)
		return qfalse;
	
	// Try to find the file in the filesystem
	len = FS_FOpenFileRead(filename, &f, qfalse);
	if (len <= 0 || !f) {
		Com_DPrintf("Lua_LoadScriptFromFS: Could not find script %s\n", filename);
		return qfalse;
	}
	
	// Allocate buffer
	buffer = (char *)Z_Malloc(len + 1);
	if (!buffer) {
		FS_FCloseFile(f);
		return qfalse;
	}
	
	// Read file
	FS_Read(buffer, len, f);
	buffer[len] = '\0';
	FS_FCloseFile(f);
	
	// Load and execute
	if (!Lua_LoadString(L, buffer)) {
		Z_Free(buffer);
		return qfalse;
	}
	
	Z_Free(buffer);
	return qtrue;
}

// =================
// CVAR Lua Bindings
// =================

/*
=================
Lua_CvarGet
=================
Lua binding: cvar_get(name) -> value
=================
*/
static int Lua_CvarGet(lua_State *L)
{
	const char *name;
	const char *value;
	
	if (lua_gettop(L) < 1) {
		lua_pushnil(L);
		return 1;
	}
	
	name = lua_tostring(L, 1);
	if (!name) {
		lua_pushnil(L);
		return 1;
	}
	
	value = Cvar_VariableString(name);
	lua_pushstring(L, value ? value : "");
	return 1;
}

/*
=================
Lua_CvarSet
=================
Lua binding: cvar_set(name, value)
=================
*/
static int Lua_CvarSet(lua_State *L)
{
	const char *name;
	const char *value;
	
	if (lua_gettop(L) < 2) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	name = lua_tostring(L, 1);
	value = lua_tostring(L, 2);
	
	if (!name || !value) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	Cvar_Set(name, value);
	lua_pushboolean(L, 1);
	return 1;
}

/*
=================
Lua_CvarGetFloat
=================
Lua binding: cvar_get_float(name) -> number
=================
*/
static int Lua_CvarGetFloat(lua_State *L)
{
	const char *name;
	float value;
	
	if (lua_gettop(L) < 1) {
		lua_pushnumber(L, 0.0);
		return 1;
	}
	
	name = lua_tostring(L, 1);
	if (!name) {
		lua_pushnumber(L, 0.0);
		return 1;
	}
	
	value = Cvar_VariableValue(name);
	lua_pushnumber(L, value);
	return 1;
}

/*
=================
Lua_CvarGetInt
=================
Lua binding: cvar_get_int(name) -> integer
=================
*/
static int Lua_CvarGetInt(lua_State *L)
{
	const char *name;
	int value;
	
	if (lua_gettop(L) < 1) {
		lua_pushinteger(L, 0);
		return 1;
	}
	
	name = lua_tostring(L, 1);
	if (!name) {
		lua_pushinteger(L, 0);
		return 1;
	}
	
	value = Cvar_VariableIntegerValue(name);
	lua_pushinteger(L, value);
	return 1;
}

/*
=================
Lua_CvarSetFloat
=================
Lua binding: cvar_set_float(name, value)
=================
*/
static int Lua_CvarSetFloat(lua_State *L)
{
	const char *name;
	float value;
	
	if (lua_gettop(L) < 2) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	name = lua_tostring(L, 1);
	if (!name || !lua_isnumber(L, 2)) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	value = (float)lua_tonumber(L, 2);
	Cvar_SetValue(name, value);
	lua_pushboolean(L, 1);
	return 1;
}

/*
=================
Lua_CvarSetInt
=================
Lua binding: cvar_set_int(name, value)
=================
*/
static int Lua_CvarSetInt(lua_State *L)
{
	const char *name;
	int value;
	
	if (lua_gettop(L) < 2) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	name = lua_tostring(L, 1);
	if (!name || !lua_isnumber(L, 2)) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	value = (int)lua_tointeger(L, 2);
	Cvar_SetIntegerValue(name, value);
	lua_pushboolean(L, 1);
	return 1;
}

// =================
// Command Lua Bindings
// =================

/*
=================
Lua_ExecuteCommand
=================
Lua binding: cmd_execute(command_string)
=================
*/
static int Lua_ExecuteCommand(lua_State *L)
{
	const char *cmd;
	
	if (lua_gettop(L) < 1) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	cmd = lua_tostring(L, 1);
	if (!cmd) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	Cmd_ExecuteString(cmd);
	lua_pushboolean(L, 1);
	return 1;
}

/*
=================
Lua_Print
=================
Lua binding: print(...) - prints to console
=================
*/
static int Lua_Print(lua_State *L)
{
	int n = lua_gettop(L);
	int i;
	const char *str;
	char buffer[1024];
	size_t pos = 0;
	
	for (i = 1; i <= n; i++) {
		if (i > 1) {
			if (pos < sizeof(buffer) - 1) {
				buffer[pos++] = ' ';
			}
		}
		
		str = lua_tostring(L, i);
		if (str) {
			size_t len = strlen(str);
			if (pos + len < sizeof(buffer) - 1) {
				Q_strncpyz(buffer + pos, str, sizeof(buffer) - pos);
				pos += len;
			}
		}
	}
	
	buffer[pos] = '\0';
	Com_Printf("%s\n", buffer);
	
	return 0;
}

/*
=================
Lua_GetMainState
Get the main Lua state (first one created)
=================
*/
lua_State *Lua_GetMainState(void)
{
	if (num_lua_states > 0) {
		return lua_states[0];
	}
	return NULL;
}

/*
=================
Lua_RegisterEngineBindings
=================
Register all engine bindings with a Lua state
=================
*/
void Lua_RegisterEngineBindings(lua_State *L)
{
	if (!L)
		return;
	
	// CVAR bindings
	Lua_RegisterFunction(L, "cvar_get", Lua_CvarGet);
	Lua_RegisterFunction(L, "cvar_set", Lua_CvarSet);
	Lua_RegisterFunction(L, "cvar_get_float", Lua_CvarGetFloat);
	Lua_RegisterFunction(L, "cvar_get_int", Lua_CvarGetInt);
	Lua_RegisterFunction(L, "cvar_set_float", Lua_CvarSetFloat);
	Lua_RegisterFunction(L, "cvar_set_int", Lua_CvarSetInt);
	
	// Command bindings
	Lua_RegisterFunction(L, "cmd_execute", Lua_ExecuteCommand);
	
	// Override Lua's print function with our console print
	Lua_RegisterFunction(L, "print", Lua_Print);
}

/*
=================
Lua_ReloadScripts
Reload all Lua scripts (hot reload)
=================
*/
void Lua_ReloadScripts(void)
{
	int i;
	lua_State *L;
	
	if (!com_lua_enabled || !com_lua_enabled->integer) {
		Com_Printf("Lua scripting is disabled\n");
		return;
	}
	
	// Reload scripts in all Lua states
	for (i = 0; i < num_lua_states; i++) {
		L = lua_states[i];
		if (!L)
			continue;
		
		// Clear package.loaded to force reload
		lua_getglobal(L, "package");
		if (lua_istable(L, -1)) {
			lua_getfield(L, -1, "loaded");
			if (lua_istable(L, -1)) {
				// Clear all loaded modules
				lua_pushnil(L);
				while (lua_next(L, -2) != 0) {
					lua_pop(L, 1);
					lua_pushnil(L);
					lua_settable(L, -3);
				}
			}
			lua_pop(L, 1);
		}
		lua_pop(L, 1);
		
		// Reload scripts from filesystem
		Com_Printf("Reloading scripts in Lua state %d...\n", i);
		Lua_LoadScriptsFromFS(L);
	}
	
	Com_Printf("Lua scripts reloaded\n");
}

/*
=================
Lua_ReloadScript_f
Console command to reload Lua scripts
=================
*/
static void Lua_ReloadScript_f(void)
{
	Lua_ReloadScripts();
}

#endif // USE_LUA

