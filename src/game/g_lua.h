/*
===========================================================================
id Tech 3 - Game LUA Interface Header

Comprehensive LUA scripting integration for game logic.
Based on ET:Legacy implementation pattern.

Features:
- Multiple LUA VM support
- Event-driven callbacks
- Entity field access
- Game state manipulation
- Console command integration
===========================================================================
*/

#ifdef USE_LUA

#ifndef INCLUDE_G_LUA_H
#define INCLUDE_G_LUA_H

#include "../common/q_shared.h"
#include "g_public.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define LUA_NUM_VM 16
#define LUA_MAX_FSIZE (1024 * 1024) // 1MB

#define FIELD_INT           0
#define FIELD_STRING        1
#define FIELD_FLOAT         2
#define FIELD_ENTITY        3
#define FIELD_VEC3          4
#define FIELD_INT_ARRAY     5
#define FIELD_TRAJECTORY    6
#define FIELD_FLOAT_ARRAY   7

#define FIELD_FLAG_GENTITY  1
#define FIELD_FLAG_GCLIENT  2
#define FIELD_FLAG_NOPTR    4
#define FIELD_FLAG_READONLY 8

// Platform-specific defines
#if defined(_WIN32)
#define HOSTARCH    "WIN32"
#define EXTENSION   "dll"
#else
#define HOSTARCH    "UNIX"
#define EXTENSION   "so"
#endif

// LUA VM structure
typedef struct {
    int id;
    char file_name[MAX_QPATH];
    char mod_name[MAX_CVAR_VALUE_STRING];
    char mod_signature[41];
    char *code;
    int code_size;
    int err;
    lua_State *L;
} lua_vm_t;

// Entity field descriptor
typedef struct {
    const char *name;
    int type;
    uintptr_t mapping;
    int flags;
} gentity_field_t;

// Print message types
typedef enum {
    GPRINT_TEXT = 0,
    GPRINT_DEVELOPER,
    GPRINT_ERROR
} printMessageType_t;

// Global VM array
extern lua_vm_t *lVM[LUA_NUM_VM];

// Core API
qboolean G_LuaInit(void);
qboolean G_LuaCall(lua_vm_t *vm, const char *func, int nargs, int nresults);
qboolean G_LuaGetNamedFunction(lua_vm_t *vm, const char *name);
qboolean G_LuaStartVM(lua_vm_t *vm);
void G_LuaStopVM(lua_vm_t *vm);
void G_LuaShutdown(void);
void G_LuaStatus(void);
lua_vm_t *G_LuaGetVM(lua_State *L);

// Console commands
void Svcmd_LoadLua_f(void);

// Event hooks (game lifecycle)
void G_LuaHook_InitGame(int levelTime, int randomSeed, int restart);
void G_LuaHook_ShutdownGame(int restart);
void G_LuaHook_RunFrame(int levelTime);

// Client hooks
qboolean G_LuaHook_ClientConnect(int clientNum, qboolean firstTime, qboolean isBot);
void G_LuaHook_ClientDisconnect(int clientNum);
void G_LuaHook_ClientBegin(int clientNum);
void G_LuaHook_ClientSpawn(int clientNum);

// Entity/field access functions
int G_LuaGetEntityField(lua_State *L);
int G_LuaSetEntityField(lua_State *L);

// Utility functions
void G_LuaStackDump(lua_State *L);
int G_LuaGetEntityId(uintptr_t addr);

#endif // INCLUDE_G_LUA_H

#endif // USE_LUA