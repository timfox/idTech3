#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_LUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <optional>
#include <string_view>
#include "ecs.h"
#include "lua_entity.h"

#ifdef USE_ENTT
#include "ecs_internal.h"
#include "ecs_components.h"
#endif

/*
=================
Lua_GameSpawnEntity
=================
Lua binding: game_spawn_entity(classname, x, y, z) -> entity_num
=================
*/
static int Lua_GameSpawnEntity(lua_State *L)
{
	const auto numArgs = lua_gettop(L);

	// Use std::optional for cleaner error handling
	std::optional<std::string_view> classname;
	vec3_t origin{};

	if (numArgs < 4) {
		lua_pushinteger(L, -1);
		return 1;
	}

	// Modern C++: use std::string_view for string handling
	if (const char* cls = lua_tostring(L, 1); cls) {
		classname = cls;
	} else {
		lua_pushinteger(L, -1);
		return 1;
	}

	// Validate and extract position coordinates
	const auto x = lua_tonumber(L, 2);
	const auto y = lua_tonumber(L, 3);
	const auto z = lua_tonumber(L, 4);

	if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) {
		lua_pushinteger(L, -1);
		return 1;
	}

	// Use structured bindings for coordinate assignment
	auto [ox, oy, oz] = std::make_tuple(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
	origin[0] = ox;
	origin[1] = oy;
	origin[2] = oz;
	
#ifdef USE_ENTT
	// Create ECS entity
	ecs_entity_t entity = ECS_CreateEntity();
	if (!ECS_IsValid(entity)) {
		lua_pushinteger(L, -1);
		return 1;
	}

	// Add TransformComponent
	entt::registry &registry = ECS::GetRegistry();
	entt::entity e = static_cast<entt::entity>(entity);
	TransformComponent transform(origin);
	registry.emplace<TransformComponent>(e, transform);

	// Return entity number
	lua_pushinteger(L, (lua_Integer)entity);
	return 1;
#else
	Com_DPrintf("Lua_GameSpawnEntity: ECS not available\n");
	lua_pushinteger(L, -1);
	return 1;
#endif
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
	
#ifdef USE_LUA
	// Emit event via event bus
	extern void Lua_Events_Emit(const char *event_name, int num_args, ...);
	Lua_Events_Emit(eventName, 1, (double)entityNum);
	
	lua_pushboolean(L, 1);
	return 1;
#else
	lua_pushboolean(L, 0);
	return 1;
#endif
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
#ifdef USE_ENTT
	extern ecs_registry_t *ECS_GetRegistry(void);
	if (ECS_GetRegistry()) {
		entt::registry &registry = ECS::GetRegistry();
		int count = (int)registry.storage<entt::entity>().size();
		lua_pushinteger(L, count);
	} else {
		lua_pushinteger(L, 0);
	}
	return 1;
#else
	lua_pushinteger(L, 0);
	return 1;
#endif
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
	
	ecs_entity_t entity = (ecs_entity_t)entityNum;
	if (ECS_IsValid(entity)) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}
	return 1;
}

/*
=================
Lua_GameEntityAttachScript
=================
Lua binding: game_entity_attach_script(entity_num, script_path) -> boolean
Attaches a Lua script to an ECS entity (alias for Entity.attach_script)
=================
*/
static int Lua_GameEntityAttachScript(lua_State *L)
{
	ecs_entity_t entity;
	const char *script_path;

	if (lua_gettop(L) < 2 || !lua_isnumber(L, 1)) {
		lua_pushboolean(L, 0);
		return 1;
	}

	entity = (ecs_entity_t)lua_tointeger(L, 1);
	script_path = lua_tostring(L, 2);
	if (!script_path) {
		lua_pushboolean(L, 0);
		return 1;
	}

	if (Lua_Entity_AttachScript(entity, script_path)) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}
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
	Lua_RegisterFunction(L, "game_entity_attach_script", Lua_GameEntityAttachScript);
	
	// Register animation event bindings (weak symbol - may not be available if game module not loaded)
	extern __attribute__((weak)) void G_RegisterAnimationEventLua( void *luaState );
	if ( G_RegisterAnimationEventLua ) {
		G_RegisterAnimationEventLua( L );
	}
	
	// Additional bindings for ECS and physics
	// Note: Entity position and script functions are available via Entity.* API
	// These are registered in lua_entity.cpp
}

#endif // USE_LUA

