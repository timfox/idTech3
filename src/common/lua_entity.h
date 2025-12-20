/*
===========================================================================
Lua Entity Script System

Attaches Lua scripts to ECS entities with lifecycle hooks.
===========================================================================
*/

#ifndef __LUA_ENTITY_H__
#define __LUA_ENTITY_H__

#include "q_shared.h"
#include "ecs.h"

#ifdef USE_LUA

#ifdef __cplusplus
extern "C" {
#include <lua.h>
}
#else
#include <lua.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
=================
Lua_Entity_Init
Initialize the entity script system
=================
*/
void Lua_Entity_Init(void);

/*
=================
Lua_Entity_Shutdown
Shutdown the entity script system
=================
*/
void Lua_Entity_Shutdown(void);

/*
=================
Lua_Entity_Update
Update all entity scripts (call OnUpdate hooks)
Should be called once per frame
deltaTime: Time since last frame in seconds
=================
*/
void Lua_Entity_Update(float deltaTime);

/*
=================
Lua_Entity_AttachScript
Attach a Lua script to an ECS entity
entity: ECS entity ID
script_path: Path to Lua script file
Returns qtrue on success
=================
*/
qboolean Lua_Entity_AttachScript(ecs_entity_t entity, const char *script_path);

/*
=================
Lua_Entity_DetachScript
Detach script from an entity
entity: ECS entity ID
=================
*/
void Lua_Entity_DetachScript(ecs_entity_t entity);

/*
=================
Lua_Entity_CallHook
Call a lifecycle hook on an entity's script
entity: ECS entity ID
hook_name: Name of hook (e.g., "OnSpawn", "OnTakeDamage")
num_args: Number of arguments to pass
...: Variable arguments
=================
*/
void Lua_Entity_CallHook(ecs_entity_t entity, const char *hook_name, int num_args, ...);

/*
=================
Lua_Entity_RegisterBindings
Register Lua bindings for entity system
L: Lua state
=================
*/
void Lua_Entity_RegisterBindings(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif // USE_LUA

#endif // __LUA_ENTITY_H__

