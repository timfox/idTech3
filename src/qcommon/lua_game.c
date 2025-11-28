#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_LUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// Forward declarations for game module functions
// These will be implemented when game module integration is added
extern void G_SpawnEntity(const char *classname, vec3_t origin, vec3_t angles);
extern void G_TriggerEvent(int entityNum, const char *eventName);
extern int G_GetEntityCount(void);
extern qboolean G_EntityExists(int entityNum);

/*
=================
Lua_GameSpawnEntity
=================
Lua binding: game_spawn_entity(classname, x, y, z) -> entity_num
=================
*/
static int Lua_GameSpawnEntity(lua_State *L)
{
	const char *classname;
	vec3_t origin;
	int numArgs = lua_gettop(L);
	
	if (numArgs < 4) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	classname = lua_tostring(L, 1);
	if (!classname) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	origin[0] = (float)lua_tonumber(L, 2);
	origin[1] = (float)lua_tonumber(L, 3);
	origin[2] = (float)lua_tonumber(L, 4);
	
	// TODO: Implement actual entity spawning when game module is integrated
	// For now, this is a placeholder
	Com_DPrintf("Lua_GameSpawnEntity: Spawning %s at (%.2f, %.2f, %.2f)\n",
		classname, origin[0], origin[1], origin[2]);
	
	// Return entity number (placeholder)
	lua_pushinteger(L, 0);
	return 1;
}

/*
=================
Lua_GameTriggerEvent
=================
Lua binding: game_trigger_event(entity_num, event_name)
=================
*/
static int Lua_GameTriggerEvent(lua_State *L)
{
	int entityNum;
	const char *eventName;
	
	if (lua_gettop(L) < 2) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	if (!lua_isnumber(L, 1)) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	entityNum = (int)lua_tointeger(L, 1);
	eventName = lua_tostring(L, 2);
	
	if (!eventName) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	// TODO: Implement actual event triggering when game module is integrated
	Com_DPrintf("Lua_GameTriggerEvent: Triggering event %s on entity %d\n",
		eventName, entityNum);
	
	lua_pushboolean(L, 1);
	return 1;
}

/*
=================
Lua_GameGetEntityCount
=================
Lua binding: game_get_entity_count() -> count
=================
*/
static int Lua_GameGetEntityCount(lua_State *L)
{
	// TODO: Implement actual entity count when game module is integrated
	int count = 0;
	lua_pushinteger(L, count);
	return 1;
}

/*
=================
Lua_GameEntityExists
=================
Lua binding: game_entity_exists(entity_num) -> boolean
=================
*/
static int Lua_GameEntityExists(lua_State *L)
{
	int entityNum;
	
	if (lua_gettop(L) < 1 || !lua_isnumber(L, 1)) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	entityNum = (int)lua_tointeger(L, 1);
	
	// TODO: Implement actual entity existence check when game module is integrated
	lua_pushboolean(L, 0);
	return 1;
}

/*
=================
Lua_RegisterGameBindings
=================
Register all game module bindings with a Lua state
=================
*/
void Lua_RegisterGameBindings(lua_State *L)
{
	if (!L)
		return;
	
	Lua_RegisterFunction(L, "game_spawn_entity", Lua_GameSpawnEntity);
	Lua_RegisterFunction(L, "game_trigger_event", Lua_GameTriggerEvent);
	Lua_RegisterFunction(L, "game_get_entity_count", Lua_GameGetEntityCount);
	Lua_RegisterFunction(L, "game_entity_exists", Lua_GameEntityExists);
}

#endif // USE_LUA

