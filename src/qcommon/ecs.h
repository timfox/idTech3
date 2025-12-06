/*
===========================================================================
ECS (Entity Component System) C Interface

Provides C-callable functions wrapping EnTT registry for use throughout
the engine. This allows gradual migration to ECS while maintaining
backwards compatibility.
===========================================================================
*/

#ifndef __ECS_H__
#define __ECS_H__

#include "q_shared.h"

typedef unsigned int ecs_entity_t;

#define ECS_NULL_ENTITY 0

#ifdef USE_ENTT

// Forward declarations
typedef struct ecs_registry_s ecs_registry_t;

// Initialization
void ECS_Init(void);
void ECS_Shutdown(void);

// Entity management
ecs_entity_t ECS_CreateEntity(void);
void ECS_DestroyEntity(ecs_entity_t entity);
qboolean ECS_IsValid(ecs_entity_t entity);

// Registry access (for C++ code)
ecs_registry_t *ECS_GetRegistry(void);

// Component management (generic)
// Note: Specific component functions will be added as needed
// These are placeholder functions for the basic interface

// Frame update
void ECS_RunFrame(float deltaTime);

// C++ helper functions (for internal use in .cpp files)
#ifdef __cplusplus
#include <entt/entt.hpp>
entt::entity ECS_GetEntityFromIndex(int index);
int ECS_GetIndexFromEntity(entt::entity entity);
void ECS_MapEntity(int index, entt::entity entity);
void ECS_UnmapEntity(int index);
#endif

#endif // USE_ENTT

#endif // __ECS_H__

