/*
===========================================================================
id Tech 3 - Game LUA Interface Implementation

Comprehensive LUA scripting integration for game logic.
Based on ET:Legacy implementation pattern.
===========================================================================
*/

#ifdef USE_LUA

#include "g_lua.h"

// Global VM storage
lua_vm_t *lVM[LUA_NUM_VM];

// CVARs for LUA control
static cvar_t *lua_enabled;
static cvar_t *lua_debug;
static cvar_t *lua_sql_enabled;

/*
===============
G_LuaGetEntityId

Convert entity pointer to entity number for LUA access
===============
*/
int G_LuaGetEntityId(uintptr_t addr)
{
    // TODO: Implement when game entities are available
    (void)addr; // Suppress unused parameter warning
    Com_Printf("G_LuaGetEntityId not implemented - no game entities available\n");
    return -1;
}

/*
===============
G_LuaGetVM

Find the VM associated with a LUA state
===============
*/
lua_vm_t *G_LuaGetVM(lua_State *L)
{
    int i;

    for (i = 0; i < LUA_NUM_VM; i++) {
        if (lVM[i] && lVM[i]->L == L) {
            return lVM[i];
        }
    }

    return NULL;
}

/*
===============
G_LuaStackDump

Debug utility to dump LUA stack
===============
*/
void G_LuaStackDump(lua_State *L)
{
    int i;
    int top = lua_gettop(L);

    Com_Printf("LUA Stack (%d items):\n", top);

    for (i = 1; i <= top; i++) {
        int t = lua_type(L, i);
        switch (t) {
            case LUA_TSTRING:
                Com_Printf("  %d: '%s'\n", i, lua_tostring(L, i));
                break;
            case LUA_TBOOLEAN:
                Com_Printf("  %d: %s\n", i, lua_toboolean(L, i) ? "true" : "false");
                break;
            case LUA_TNUMBER:
                Com_Printf("  %d: %g\n", i, lua_tonumber(L, i));
                break;
            default:
                Com_Printf("  %d: %s\n", i, lua_typename(L, t));
                break;
        }
    }
}

/*
===============
G_LuaInit

Initialize the LUA system
===============
*/
qboolean G_LuaInit(void)
{

    // Register CVARs
    lua_enabled = Cvar_Get("lua_enabled", "1", CVAR_ARCHIVE);
    lua_debug = Cvar_Get("lua_debug", "0", CVAR_TEMP);
    lua_sql_enabled = Cvar_Get("lua_sql_enabled", "1", CVAR_ARCHIVE);

    if (!lua_enabled->integer) {
        Com_Printf("LUA system disabled by cvar\n");
        return qtrue;
    }

    // Initialize VM array
    Com_Memset(lVM, 0, sizeof(lVM));

    Com_Printf("LUA system initialized with %d VM slots\n", LUA_NUM_VM);
    return qtrue;
}

/*
===============
G_LuaShutdown

Shutdown the LUA system
===============
*/
void G_LuaShutdown(void)
{
    int i;

    for (i = 0; i < LUA_NUM_VM; i++) {
        if (lVM[i]) {
            G_LuaStopVM(lVM[i]);
        }
    }

    Com_Printf("LUA system shutdown\n");
}

/*
===============
G_LuaStartVM

Start a LUA VM with loaded code
===============
*/
qboolean G_LuaStartVM(lua_vm_t *vm)
{
    int error;

    if (!vm || !vm->code || !vm->code_size) {
        Com_Printf("G_LuaStartVM: invalid VM or no code loaded\n");
        return qfalse;
    }

    // Create new LUA state
    vm->L = luaL_newstate();
    if (!vm->L) {
        Com_Printf("G_LuaStartVM: failed to create LUA state\n");
        return qfalse;
    }

    // Open standard libraries
    luaL_openlibs(vm->L);

    // Register LuaSQL if enabled
    if (lua_sql_enabled && lua_sql_enabled->integer) {
        luaopen_luasql_sqlite3(vm->L);
        lua_setglobal(vm->L, "luasql");
    }

    // TODO: Register game-specific functions
    // G_LuaRegisterGameFunctions(vm->L);

    // Load and execute the code
    error = luaL_loadbuffer(vm->L, vm->code, vm->code_size, vm->file_name);
    if (error) {
        Com_Printf("G_LuaStartVM: failed to load %s: %s\n",
                 vm->file_name, lua_tostring(vm->L, -1));
        lua_pop(vm->L, 1);
        return qfalse;
    }

    // Execute the loaded chunk
    error = lua_pcall(vm->L, 0, LUA_MULTRET, 0);
    if (error) {
        Com_Printf("G_LuaStartVM: failed to execute %s: %s\n",
                 vm->file_name, lua_tostring(vm->L, -1));
        lua_pop(vm->L, 1);
        return qfalse;
    }

    if (lua_debug->integer) {
        Com_Printf("LUA VM %d started: %s\n", vm->id, vm->file_name);
    }

    return qtrue;
}

/*
===============
G_LuaStopVM

Stop and cleanup a LUA VM
===============
*/
void G_LuaStopVM(lua_vm_t *vm)
{
    if (!vm) return;

    if (vm->L) {
        lua_close(vm->L);
        vm->L = NULL;
    }

    if (vm->code) {
        Z_Free(vm->code);
        vm->code = NULL;
    }

    vm->code_size = 0;
    vm->err = 0;

    if (lua_debug->integer) {
        Com_Printf("LUA VM %d stopped\n", vm->id);
    }
}

/*
===============
G_LuaCall

Call a LUA function
===============
*/
qboolean G_LuaCall(lua_vm_t *vm, const char *func, int nargs, int nresults)
{
    if (!vm || !vm->L) {
        return qfalse;
    }

    // Get the function
    if (!G_LuaGetNamedFunction(vm, func)) {
        return qfalse;
    }

    // Call it
    if (lua_pcall(vm->L, nargs, nresults, 0) != LUA_OK) {
        Com_Printf("LUA error calling %s: %s\n", func, lua_tostring(vm->L, -1));
        lua_pop(vm->L, 1);
        return qfalse;
    }

    return qtrue;
}

/*
===============
G_LuaGetNamedFunction

Get a named function from LUA global table
===============
*/
qboolean G_LuaGetNamedFunction(lua_vm_t *vm, const char *name)
{
    if (!vm || !vm->L || !name) {
        return qfalse;
    }

    lua_getglobal(vm->L, name);
    if (!lua_isfunction(vm->L, -1)) {
        lua_pop(vm->L, 1);
        return qfalse;
    }

    return qtrue;
}

/*
===============
G_LuaStatus

Print LUA VM status
===============
*/
void G_LuaStatus(void)
{
    int i, active = 0;

    for (i = 0; i < LUA_NUM_VM; i++) {
        if (lVM[i]) {
            active++;
        }
    }

    Com_Printf("LUA Status: %d/%d VMs active\n", active, LUA_NUM_VM);

    for (i = 0; i < LUA_NUM_VM; i++) {
        if (lVM[i]) {
            Com_Printf("  VM %d: %s (%s)\n", i, lVM[i]->file_name, lVM[i]->mod_name);
        }
    }
}

/*
===============
Svcmd_LoadLua_f

Console command to load LUA scripts
===============
*/
void Svcmd_LoadLua_f(void)
{
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: loadlua <script_name>\n");
        return;
    }

    const char* script_name = Cmd_Argv(1);

    // Load the script file
    fileHandle_t file = FS_FOpenFileRead(script_name, NULL, qfalse);
    if (!file) {
        Com_Printf("Svcmd_LoadLua_f: Could not open script '%s'\n", script_name);
        return;
    }

    // Read the script content
    char* script_content = NULL;
    int file_len = FS_ReadFile(script_name, (void**)&script_content);
    if (file_len <= 0 || !script_content) {
        FS_FCloseFile(file);
        Com_Printf("Svcmd_LoadLua_f: Could not read script '%s'\n", script_name);
        return;
    }

    FS_FCloseFile(file);

    // Create a new LUA state for this script
    lua_State* L = luaL_newstate();
    if (!L) {
        Com_Printf("Svcmd_LoadLua_f: Could not create LUA state for '%s'\n", script_name);
        FS_FreeFile(script_content);
        return;
    }

    // Open standard libraries
    luaL_openlibs(L);

    // Register game functions
    G_LuaRegisterFunctions(L);

    // Load and execute the script
    if (luaL_loadbuffer(L, script_content, file_len, script_name) != LUA_OK) {
        Com_Printf("Svcmd_LoadLua_f: Failed to load script '%s': %s\n",
                  script_name, lua_tostring(L, -1));
        lua_close(L);
        FS_FreeFile(script_content);
        return;
    }

    // Execute the script
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        Com_Printf("Svcmd_LoadLua_f: Failed to execute script '%s': %s\n",
                  script_name, lua_tostring(L, -1));
        lua_close(L);
        FS_FreeFile(script_content);
        return;
    }

    // Store the script state (in a real implementation, this would be managed properly)
    // For now, we'll just close it since we don't have script management yet
    lua_close(L);

    Com_Printf("Svcmd_LoadLua_f: Successfully loaded script '%s' (%d bytes)\n",
              script_name, file_len);

    FS_FreeFile(script_content);
}

/*
===============
LUA Hook Functions

These are called from game events to allow LUA scripts to respond
===============
*/

void G_LuaHook_InitGame(int levelTime, int randomSeed, int restart)
{
    int i;

    for (i = 0; i < LUA_NUM_VM; i++) {
        if (lVM[i] && lVM[i]->L) {
            lua_getglobal(lVM[i]->L, "et_InitGame");
            if (lua_isfunction(lVM[i]->L, -1)) {
                lua_pushinteger(lVM[i]->L, levelTime);
                lua_pushinteger(lVM[i]->L, randomSeed);
                lua_pushboolean(lVM[i]->L, restart);

                if (lua_pcall(lVM[i]->L, 3, 0, 0) != LUA_OK) {
                    Com_Printf("LUA et_InitGame error: %s\n", lua_tostring(lVM[i]->L, -1));
                    lua_pop(lVM[i]->L, 1);
                }
            } else {
                lua_pop(lVM[i]->L, 1);
            }
        }
    }
}

void G_LuaHook_ShutdownGame(int restart)
{
    int i;

    for (i = 0; i < LUA_NUM_VM; i++) {
        if (lVM[i] && lVM[i]->L) {
            lua_getglobal(lVM[i]->L, "et_ShutdownGame");
            if (lua_isfunction(lVM[i]->L, -1)) {
                lua_pushboolean(lVM[i]->L, restart);

                if (lua_pcall(lVM[i]->L, 1, 0, 0) != LUA_OK) {
                    Com_Printf("LUA et_ShutdownGame error: %s\n", lua_tostring(lVM[i]->L, -1));
                    lua_pop(lVM[i]->L, 1);
                }
            } else {
                lua_pop(lVM[i]->L, 1);
            }
        }
    }
}

void G_LuaHook_RunFrame(int levelTime)
{
    int i;

    for (i = 0; i < LUA_NUM_VM; i++) {
        if (lVM[i] && lVM[i]->L) {
            lua_getglobal(lVM[i]->L, "et_RunFrame");
            if (lua_isfunction(lVM[i]->L, -1)) {
                lua_pushinteger(lVM[i]->L, levelTime);

                if (lua_pcall(lVM[i]->L, 1, 0, 0) != LUA_OK) {
                    Com_Printf("LUA et_RunFrame error: %s\n", lua_tostring(lVM[i]->L, -1));
                    lua_pop(lVM[i]->L, 1);
                }
            } else {
                lua_pop(lVM[i]->L, 1);
            }
        }
    }
}

qboolean G_LuaHook_ClientConnect(int clientNum, qboolean firstTime, qboolean isBot)
{
    int i;
    qboolean result = qtrue;

    for (i = 0; i < LUA_NUM_VM; i++) {
        if (lVM[i] && lVM[i]->L) {
            lua_getglobal(lVM[i]->L, "et_ClientConnect");
            if (lua_isfunction(lVM[i]->L, -1)) {
                lua_pushinteger(lVM[i]->L, clientNum);
                lua_pushboolean(lVM[i]->L, firstTime);
                lua_pushboolean(lVM[i]->L, isBot);

                if (lua_pcall(lVM[i]->L, 3, 1, 0) != LUA_OK) {
                    Com_Printf("LUA et_ClientConnect error: %s\n", lua_tostring(lVM[i]->L, -1));
                    lua_pop(lVM[i]->L, 1);
                } else {
                    if (lua_isboolean(lVM[i]->L, -1)) {
                        result = result && lua_toboolean(lVM[i]->L, -1);
                    }
                    lua_pop(lVM[i]->L, 1);
                }
            } else {
                lua_pop(lVM[i]->L, 1);
            }
        }
    }

    return result;
}

void G_LuaHook_ClientDisconnect(int clientNum)
{
    int i;

    for (i = 0; i < LUA_NUM_VM; i++) {
        if (lVM[i] && lVM[i]->L) {
            lua_getglobal(lVM[i]->L, "et_ClientDisconnect");
            if (lua_isfunction(lVM[i]->L, -1)) {
                lua_pushinteger(lVM[i]->L, clientNum);

                if (lua_pcall(lVM[i]->L, 1, 0, 0) != LUA_OK) {
                    Com_Printf("LUA et_ClientDisconnect error: %s\n", lua_tostring(lVM[i]->L, -1));
                    lua_pop(lVM[i]->L, 1);
                }
            } else {
                lua_pop(lVM[i]->L, 1);
            }
        }
    }
}

void G_LuaHook_ClientBegin(int clientNum)
{
    int i;

    for (i = 0; i < LUA_NUM_VM; i++) {
        if (lVM[i] && lVM[i]->L) {
            lua_getglobal(lVM[i]->L, "et_ClientBegin");
            if (lua_isfunction(lVM[i]->L, -1)) {
                lua_pushinteger(lVM[i]->L, clientNum);

                if (lua_pcall(lVM[i]->L, 1, 0, 0) != LUA_OK) {
                    Com_Printf("LUA et_ClientBegin error: %s\n", lua_tostring(lVM[i]->L, -1));
                    lua_pop(lVM[i]->L, 1);
                }
            } else {
                lua_pop(lVM[i]->L, 1);
            }
        }
    }
}

void G_LuaHook_ClientSpawn(int clientNum)
{
    int i;

    for (i = 0; i < LUA_NUM_VM; i++) {
        if (lVM[i] && lVM[i]->L) {
            lua_getglobal(lVM[i]->L, "et_ClientSpawn");
            if (lua_isfunction(lVM[i]->L, -1)) {
                lua_pushinteger(lVM[i]->L, clientNum);

                if (lua_pcall(lVM[i]->L, 1, 0, 0) != LUA_OK) {
                    Com_Printf("LUA et_ClientSpawn error: %s\n", lua_tostring(lVM[i]->L, -1));
                    lua_pop(lVM[i]->L, 1);
                }
            } else {
                lua_pop(lVM[i]->L, 1);
            }
        }
    }
}

/*
===============
G_LuaGetEntityField

LUA function to get entity field values
===============
*/
int G_LuaGetEntityField(lua_State *L)
{
    // Get parameters: entity_index, field_name
    int entity_index = luaL_checkinteger(L, 1);
    const char* field_name = luaL_checkstring(L, 2);

    // Validate entity index
    if (entity_index < 0 || entity_index >= level.num_entities) {
        luaL_error(L, "Invalid entity index: %d", entity_index);
        return 0;
    }

    gentity_t* ent = &g_entities[entity_index];

    // Handle common entity fields
    if (strcmp(field_name, "classname") == 0) {
        lua_pushstring(L, ent->classname);
        return 1;
    } else if (strcmp(field_name, "origin") == 0) {
        lua_newtable(L);
        vec3_t origin;
        G_GetOrigin(ent, origin);
        lua_pushnumber(L, origin[0]); lua_rawseti(L, -2, 1);
        lua_pushnumber(L, origin[1]); lua_rawseti(L, -2, 2);
        lua_pushnumber(L, origin[2]); lua_rawseti(L, -2, 3);
        return 1;
    } else if (strcmp(field_name, "angles") == 0) {
        lua_newtable(L);
        vec3_t angles;
        G_GetAngles(ent, angles);
        lua_pushnumber(L, angles[0]); lua_rawseti(L, -2, 1);
        lua_pushnumber(L, angles[1]); lua_rawseti(L, -2, 2);
        lua_pushnumber(L, angles[2]); lua_rawseti(L, -2, 3);
        return 1;
    } else if (strcmp(field_name, "health") == 0) {
        if (ent->client) {
            lua_pushinteger(L, ent->client->ps.stats[STAT_HEALTH]);
        } else {
            lua_pushinteger(L, ent->health);
        }
        return 1;
    } else if (strcmp(field_name, "inuse") == 0) {
        lua_pushboolean(L, ent->inuse);
        return 1;
    }

    // Field not found
    lua_pushnil(L);
    return 1;
}

/*
===============
G_LuaSetEntityField

LUA function to set entity field values
===============
*/
int G_LuaSetEntityField(lua_State *L)
{
    // Get parameters: entity_index, field_name, value
    int entity_index = luaL_checkinteger(L, 1);
    const char* field_name = luaL_checkstring(L, 2);

    // Validate entity index
    if (entity_index < 0 || entity_index >= level.num_entities) {
        luaL_error(L, "Invalid entity index: %d", entity_index);
        return 0;
    }

    gentity_t* ent = &g_entities[entity_index];

    // Handle common entity fields
    if (strcmp(field_name, "origin") == 0) {
        if (lua_istable(L, 3)) {
            vec3_t origin;
            lua_rawgeti(L, 3, 1); origin[0] = lua_tonumber(L, -1); lua_pop(L, 1);
            lua_rawgeti(L, 3, 2); origin[1] = lua_tonumber(L, -1); lua_pop(L, 1);
            lua_rawgeti(L, 3, 3); origin[2] = lua_tonumber(L, -1); lua_pop(L, 1);
            G_SetOrigin(ent, origin);
        }
    } else if (strcmp(field_name, "angles") == 0) {
        if (lua_istable(L, 3)) {
            vec3_t angles;
            lua_rawgeti(L, 3, 1); angles[0] = lua_tonumber(L, -1); lua_pop(L, 1);
            lua_rawgeti(L, 3, 2); angles[1] = lua_tonumber(L, -1); lua_pop(L, 1);
            lua_rawgeti(L, 3, 3); angles[2] = lua_tonumber(L, -1); lua_pop(L, 1);
            G_SetAngles(ent, angles);
        }
    } else if (strcmp(field_name, "health") == 0) {
        int health = luaL_checkinteger(L, 3);
        if (ent->client) {
            ent->client->ps.stats[STAT_HEALTH] = health;
        } else {
            ent->health = health;
        }
    } else if (strcmp(field_name, "classname") == 0) {
        const char* classname = luaL_checkstring(L, 3);
        Q_strncpyz(ent->classname, classname, sizeof(ent->classname));
    }

    return 0; // No return values
}

#endif // USE_LUA