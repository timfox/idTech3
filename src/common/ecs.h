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

// Forward declarations for EntityPlus-inspired components
#ifdef __cplusplus
enum class KeyColor : uint8_t;
enum class ItemType : uint8_t;
#endif

// Forward declarations
typedef struct ecs_registry_s ecs_registry_t;

// Initialization
void ECS_Init(void);
void ECS_Shutdown(void);

// Entity management
ecs_entity_t ECS_CreateEntity(void);
void ECS_DestroyEntity(ecs_entity_t entity);
qboolean ECS_IsValid(ecs_entity_t entity);

// C++23 std::optional version (for modern C++ code)
#ifdef __cplusplus
#include <optional>
std::optional<ecs_entity_t> ECS_CreateEntity_Optional(void);
#endif

// Registry access (for C++ code)
ecs_registry_t *ECS_GetRegistry(void);

// Component management (generic)
// Note: Specific component functions will be added as needed
// These are placeholder functions for the basic interface

// Component helpers
qboolean ECS_SetTransform(ecs_entity_t entity, const vec3_t position, const vec3_t rotation, const vec3_t scale);
qboolean ECS_SetPhysics(ecs_entity_t entity, const vec3_t velocity, const vec3_t acceleration, float mass, float friction, qboolean useBullet);
qboolean ECS_SetHealth(ecs_entity_t entity, int health, int maxHealth, int armor, int maxArmor);
qboolean ECS_SetLifetime(ecs_entity_t entity, float seconds);
void ECS_ClearLifetime(ecs_entity_t entity);

// EntityPlus-inspired entity creation helpers
#ifdef __cplusplus
// Note: These functions use enum classes from ecs_components.h
// For C compatibility, use int values that can be cast to KeyColor/ItemType
ecs_entity_t ECS_CreateKey(int color, int keyId, qboolean isKeycard, const vec3_t position);
ecs_entity_t ECS_CreateBackpack(const vec3_t position, int inventorySlots, int ammoCapacity, qboolean permanent);
ecs_entity_t ECS_CreateObjective(int objectiveId, const char *title, const char *description, const vec3_t position);
ecs_entity_t ECS_CreateDebris(const vec3_t position, const vec3_t velocity, const char *model, const char *material, float lifetime);
ecs_entity_t ECS_CreatePickupItem(int itemType, int itemId, const vec3_t position, const char *model);

// Class-based gameplay helpers
ecs_entity_t ECS_CreatePlayerClass(int classType, int team, const vec3_t position);
qboolean ECS_AddSkillXP(ecs_entity_t entity, int skillType, int xpAmount);
qboolean ECS_JoinFireteam(ecs_entity_t entity, int fireteamId);
qboolean ECS_LeaveFireteam(ecs_entity_t entity);
int ECS_CreateFireteam(const char *name, int leaderEntityId, int team);
#endif

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

