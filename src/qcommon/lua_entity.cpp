/*
===========================================================================
Lua Entity Script System Implementation

Attaches Lua scripts to ECS entities with lifecycle hooks.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_LUA

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "lua_entity.h"
#include "lua_events.h"
#ifdef USE_ENTT
#include "ecs_components.h"
#include "ecs_internal.h"
#endif
#include <string.h>

static qboolean s_initialized = qfalse;

/*
=================
Lua_Entity_Init
Initialize the entity script system
=================
*/
void Lua_Entity_Init(void)
{
	if (s_initialized) {
		return;
	}
	s_initialized = qtrue;
}

/*
=================
Lua_Entity_Shutdown
Shutdown the entity script system
=================
*/
void Lua_Entity_Shutdown(void)
{
	if (!s_initialized) {
		return;
	}
	s_initialized = qfalse;
}

/*
=================
LoadEntityScript
Load a Lua script file and return reference to script table
=================
*/
static int LoadEntityScript(lua_State *L, const char *script_path)
{
	int result;
	char full_path[MAX_QPATH];

	if (!L || !script_path) {
		return -1;
	}

	// Try loading script
	Com_sprintf(full_path, sizeof(full_path), "scripts/%s", script_path);
	result = luaL_loadfile(L, full_path);
	if (result != LUA_OK) {
		const char *error = lua_tostring(L, -1);
		Com_Printf("Lua_Entity: Error loading script %s: %s\n", script_path, error ? error : "Unknown error");
		lua_pop(L, 1);
		return -1;
	}

	// Execute script (should return a table)
	result = lua_pcall(L, 0, 1, 0);
	if (result != LUA_OK) {
		const char *error = lua_tostring(L, -1);
		Com_Printf("Lua_Entity: Error executing script %s: %s\n", script_path, error ? error : "Unknown error");
		lua_pop(L, 1);
		return -1;
	}

	// Check if result is a table
	if (!lua_istable(L, -1)) {
		Com_Printf("Lua_Entity: Script %s must return a table\n", script_path);
		lua_pop(L, 1);
		return -1;
	}

	// Create reference to script table
	return luaL_ref(L, LUA_REGISTRYINDEX);
}

/*
=================
CheckScriptHook
Check if script has a specific hook function
=================
*/
static qboolean CheckScriptHook(lua_State *L, int script_ref, const char *hook_name)
{
	if (!L || script_ref < 0) {
		return qfalse;
	}

	lua_rawgeti(L, LUA_REGISTRYINDEX, script_ref);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return qfalse;
	}

	lua_getfield(L, -1, hook_name);
	qboolean has_hook = (qboolean)lua_isfunction(L, -1);
	lua_pop(L, 2);  // Pop function and table

	return has_hook;
}

/*
=================
Lua_Entity_AttachScript
Attach a Lua script to an ECS entity
=================
*/
qboolean Lua_Entity_AttachScript(ecs_entity_t entity, const char *script_path)
{
#ifdef USE_ENTT
	entt::registry &registry = ECS::GetRegistry();
	entt::entity e = static_cast<entt::entity>(entity);

	if (!ECS_IsValid(entity) || !script_path) {
		return qfalse;
	}

	// Get or create ScriptComponent
	if (!registry.all_of<ScriptComponent>(e)) {
		registry.emplace<ScriptComponent>(e);
	}

	auto &script = registry.get<ScriptComponent>(e);

	// Get Lua state (use first available)
	// For now, we'll need to get it from the Lua wrapper
	// This is a limitation - we assume scripts use the main Lua state
	extern lua_State *Lua_GetMainState(void);
	lua_State *L = Lua_GetMainState();
	if (!L) {
		extern lua_State *Lua_CreateState(void);
		L = Lua_CreateState();
	}

	if (!L) {
		return qfalse;
	}

	// Load script
	int script_ref = LoadEntityScript(L, script_path);
	if (script_ref < 0) {
		return qfalse;
	}

	// Store script reference and path
	Q_strncpyz(script.script_path, script_path, sizeof(script.script_path));
	script.script_ref = script_ref;

	// Check which hooks are available
	script.has_on_spawn = CheckScriptHook(L, script_ref, "OnSpawn");
	script.has_on_update = CheckScriptHook(L, script_ref, "OnUpdate");
	script.has_on_take_damage = CheckScriptHook(L, script_ref, "OnTakeDamage");
	script.has_on_use = CheckScriptHook(L, script_ref, "OnUse");
	script.has_on_death = CheckScriptHook(L, script_ref, "OnDeath");

	// Call OnSpawn hook if present
	if (script.has_on_spawn) {
		Lua_Entity_CallHook(entity, "OnSpawn", 0);
	}

	return qtrue;
#else
	(void)entity;
	(void)script_path;
	return qfalse;
#endif
}

/*
=================
Lua_Entity_DetachScript
Detach script from an entity
=================
*/
void Lua_Entity_DetachScript(ecs_entity_t entity)
{
#ifdef USE_ENTT
	entt::registry &registry = ECS::GetRegistry();
	entt::entity e = static_cast<entt::entity>(entity);

	if (!ECS_IsValid(entity) || !registry.all_of<ScriptComponent>(e)) {
		return;
	}

	auto &script = registry.get<ScriptComponent>(e);

	// Get Lua state
	extern lua_State *Lua_GetMainState(void);
	lua_State *L = Lua_GetMainState();
	if (L && script.script_ref >= 0) {
		luaL_unref(L, LUA_REGISTRYINDEX, script.script_ref);
	}

	registry.remove<ScriptComponent>(e);
#endif
}

/*
=================
Lua_Entity_CallHook
Call a lifecycle hook on an entity's script
=================
*/
void Lua_Entity_CallHook(ecs_entity_t entity, const char *hook_name, int num_args, ...)
{
#ifdef USE_ENTT
	entt::registry &registry = ECS::GetRegistry();
	entt::entity e = static_cast<entt::entity>(entity);

	if (!ECS_IsValid(entity) || !registry.all_of<ScriptComponent>(e) || !hook_name) {
		return;
	}

	auto &script = registry.get<ScriptComponent>(e);
	if (script.script_ref < 0) {
		return;
	}

	// Get Lua state
	extern lua_State *Lua_GetMainState(void);
	lua_State *L = Lua_GetMainState();
	if (!L) {
		return;
	}
	if (!L) {
		return;
	}

	// Get script table
	lua_rawgeti(L, LUA_REGISTRYINDEX, script.script_ref);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return;
	}

	// Get hook function
	lua_getfield(L, -1, hook_name);
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);  // Pop function and table
		return;
	}

	// Push entity ID as first argument
	lua_pushinteger(L, (lua_Integer)entity);

	// Push additional arguments
	va_list args;
	va_start(args, num_args);
	for (int i = 0; i < num_args; i++) {
		// For now, support basic types
		// This can be extended
		double num = va_arg(args, double);
		lua_pushnumber(L, num);
	}
	va_end(args);

	// Call hook (entity, ...args)
	int total_args = num_args + 1;
	if (lua_pcall(L, total_args, 0, 0) != LUA_OK) {
		const char *error = lua_tostring(L, -1);
		Com_Printf("Lua_Entity_CallHook: Error in hook %s for entity %u: %s\n",
			hook_name, (unsigned int)entity, error ? error : "Unknown error");
		lua_pop(L, 1);
	}

	// Pop script table
	lua_pop(L, 1);
#else
	(void)entity;
	(void)hook_name;
	(void)num_args;
#endif
}

/*
=================
Lua_Entity_Update
Update all entity scripts (call OnUpdate hooks)
=================
*/
void Lua_Entity_Update(float deltaTime)
{
#ifdef USE_ENTT
	entt::registry &registry = ECS::GetRegistry();
	auto view = registry.view<ScriptComponent>();

	// Get Lua state
	extern lua_State *Lua_GetMainState(void);
	lua_State *L = Lua_GetMainState();
	if (!L) {
		return;
	}
	if (!L) {
		return;
	}

	for (auto entity : view) {
		auto &script = view.get<ScriptComponent>(entity);
		
		if (!script.has_on_update || script.script_ref < 0) {
			continue;
		}

		// Get script table
		lua_rawgeti(L, LUA_REGISTRYINDEX, script.script_ref);
		if (!lua_istable(L, -1)) {
			lua_pop(L, 1);
			continue;
		}

		// Get OnUpdate function
		lua_getfield(L, -1, "OnUpdate");
		if (!lua_isfunction(L, -1)) {
			lua_pop(L, 2);  // Pop function and table
			continue;
		}

		// Push entity ID and deltaTime
		lua_pushinteger(L, (lua_Integer)static_cast<entt::entity>(entity));
		lua_pushnumber(L, deltaTime);

		// Call OnUpdate(entity, deltaTime)
		if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
			const char *error = lua_tostring(L, -1);
			Com_Printf("Lua_Entity_Update: Error in OnUpdate for entity: %s\n",
				error ? error : "Unknown error");
			lua_pop(L, 1);
		}

		// Pop script table
		lua_pop(L, 1);
	}
#else
	(void)deltaTime;
#endif
}

// =================
// Lua Bindings
// =================

/*
=================
Lua_Entity_AttachScript_Lua
Lua binding: Entity.attach_script(entity_id, script_path)
=================
*/
static int Lua_Entity_AttachScript_Lua(lua_State *L)
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
Lua_Entity_RegisterBindings
Register Lua bindings for entity system
=================
*/
void Lua_Entity_RegisterBindings(lua_State *L)
{
	if (!L) {
		return;
	}

	// Create Entity table
	lua_newtable(L);

	// Register functions
	Lua_RegisterFunction(L, "attach_script", Lua_Entity_AttachScript_Lua);

	// Set as global
	lua_setglobal(L, "Entity");
}

#endif // USE_LUA

