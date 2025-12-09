/*
===========================================================================
ECS (Entity Component System) C++ Implementation

Core EnTT registry wrapper providing C interface for engine use.
===========================================================================
*/

#ifdef USE_ENTT

#include "ecs.h"
#include "ecs_components.h"
#include "ecs_internal.h"
#include <entt/entt.hpp>
#include <unordered_map>
#include <cassert>

// Global registry instance
static entt::registry *g_registry = nullptr;

// Entity ID mapping (for compatibility with engine entity indices)
// Maps engine entity index -> EnTT entity
static std::unordered_map<int, entt::entity> g_entityMap;
static std::unordered_map<entt::entity, int> g_reverseEntityMap;
static entt::entity g_nextEntity = entt::null;

// Helper to validate registry/entity handles from C-callable wrappers.
static bool ECS_GetRegistryAndEntity(ecs_entity_t entity, entt::registry **outRegistry, entt::entity *outEntity) {
	if (g_registry == nullptr) {
		return false;
	}

	entt::entity enttEntity = static_cast<entt::entity>(entity);
	if (!g_registry->valid(enttEntity)) {
		return false;
	}

	if (outRegistry) {
		*outRegistry = g_registry;
	}
	if (outEntity) {
		*outEntity = enttEntity;
	}
	return true;
}

/*
================
ECS_Init
Initialize the ECS system
================
*/
void ECS_Init(void) {
	if (g_registry != nullptr) {
		return; // Already initialized
	}
	
	g_registry = new entt::registry();
	g_entityMap.clear();
	g_reverseEntityMap.clear();
	g_nextEntity = entt::null;
}

/*
================
ECS_Shutdown
Shutdown the ECS system
================
*/
void ECS_Shutdown(void) {
	if (g_registry == nullptr) {
		return;
	}
	
	// Clear all entities
	g_registry->clear();
	g_entityMap.clear();
	g_reverseEntityMap.clear();
	
	delete g_registry;
	g_registry = nullptr;
}

/*
================
ECS_CreateEntity
Create a new ECS entity
================
*/
ecs_entity_t ECS_CreateEntity(void) {
	if (g_registry == nullptr) {
		return ECS_NULL_ENTITY;
	}
	
	entt::entity entity = g_registry->create();
	return static_cast<ecs_entity_t>(entity);
}

/*
================
ECS_DestroyEntity
Destroy an ECS entity
================
*/
void ECS_DestroyEntity(ecs_entity_t entity) {
	if (g_registry == nullptr) {
		return;
	}
	
	entt::entity enttEntity = static_cast<entt::entity>(entity);
	
	// Remove from reverse mapping if exists
	auto it = g_reverseEntityMap.find(enttEntity);
	if (it != g_reverseEntityMap.end()) {
		g_entityMap.erase(it->second);
		g_reverseEntityMap.erase(it);
	}

#ifdef USE_BULLET
	// Tear down Bullet state before destroying the entity so bodies do not leak or keep simulating.
	if (g_registry->valid(enttEntity) && g_registry->all_of<PhysicsComponent>(enttEntity)) {
		auto &physics = g_registry->get<PhysicsComponent>(enttEntity);
		ECS::BulletOnEntityDestroyed(*g_registry, enttEntity, physics);
	}
#endif
	
	g_registry->destroy(enttEntity);
}

/*
================
ECS_IsValid
Check if an entity is valid
================
*/
qboolean ECS_IsValid(ecs_entity_t entity) {
	if (g_registry == nullptr) {
		return qfalse;
	}
	
	entt::entity enttEntity = static_cast<entt::entity>(entity);
	return g_registry->valid(enttEntity) ? qtrue : qfalse;
}

/*
================
ECS_SetTransform
Ensure a TransformComponent exists and update its values
================
*/
qboolean ECS_SetTransform(ecs_entity_t entity, const vec3_t position, const vec3_t rotation, const vec3_t scale) {
	entt::registry *registry = nullptr;
	entt::entity enttEntity = entt::null;
	if (!ECS_GetRegistryAndEntity(entity, &registry, &enttEntity)) {
		return qfalse;
	}

	TransformComponent *transform = registry->try_get<TransformComponent>(enttEntity);
	if (!transform) {
		transform = &registry->emplace<TransformComponent>(enttEntity);
	}

	VectorCopy(position, transform->position);
	VectorCopy(rotation, transform->rotation);
	VectorCopy(scale, transform->scale);

	if (auto net = registry->try_get<NetworkComponent>(enttEntity)) {
		net->needsSync = qtrue;
	}

	return qtrue;
}

/*
================
ECS_SetPhysics
Ensure a PhysicsComponent exists and update its values
================
*/
qboolean ECS_SetPhysics(ecs_entity_t entity, const vec3_t velocity, const vec3_t acceleration, float mass, float friction, qboolean useBullet) {
	entt::registry *registry = nullptr;
	entt::entity enttEntity = entt::null;
	if (!ECS_GetRegistryAndEntity(entity, &registry, &enttEntity)) {
		return qfalse;
	}

	PhysicsComponent *physics = registry->try_get<PhysicsComponent>(enttEntity);
	if (!physics) {
		physics = &registry->emplace<PhysicsComponent>(enttEntity);
	}

	VectorCopy(velocity, physics->velocity);
	VectorCopy(acceleration, physics->acceleration);
	physics->mass = mass;
	physics->friction = friction;

#ifdef USE_BULLET
	// If Bullet was previously enabled and we are turning it off, tear down the body.
	const bool disableBullet = physics->useBullet && (useBullet == qfalse);
	physics->useBullet = useBullet;
	if (disableBullet && physics->body) {
		ECS::BulletOnEntityDestroyed(*registry, enttEntity, *physics);
	}
#else
	(void)useBullet;
#endif

	if (auto net = registry->try_get<NetworkComponent>(enttEntity)) {
		net->needsSync = qtrue;
	}

	return qtrue;
}

/*
================
ECS_SetHealth
Ensure a HealthComponent exists and update its values
================
*/
qboolean ECS_SetHealth(ecs_entity_t entity, int health, int maxHealth, int armor, int maxArmor) {
	entt::registry *registry = nullptr;
	entt::entity enttEntity = entt::null;
	if (!ECS_GetRegistryAndEntity(entity, &registry, &enttEntity)) {
		return qfalse;
	}

	HealthComponent *hc = registry->try_get<HealthComponent>(enttEntity);
	if (!hc) {
		hc = &registry->emplace<HealthComponent>(enttEntity);
	}

	hc->health = health;
	hc->maxHealth = maxHealth;
	hc->armor = armor;
	hc->maxArmor = maxArmor;

	// Clamp to valid ranges
	if (hc->maxHealth < 1) hc->maxHealth = 1;
	if (hc->health < 0) hc->health = 0;
	if (hc->health > hc->maxHealth) hc->health = hc->maxHealth;

	if (hc->maxArmor < 0) hc->maxArmor = 0;
	if (hc->armor < 0) hc->armor = 0;
	if (hc->armor > hc->maxArmor) hc->armor = hc->maxArmor;

	if (auto net = registry->try_get<NetworkComponent>(enttEntity)) {
		net->needsSync = qtrue;
	}

	return qtrue;
}

/*
================
ECS_SetLifetime
Attach or update a LifetimeComponent on the entity
================
*/
qboolean ECS_SetLifetime(ecs_entity_t entity, float seconds) {
	entt::registry *registry = nullptr;
	entt::entity enttEntity = entt::null;
	if (!ECS_GetRegistryAndEntity(entity, &registry, &enttEntity)) {
		return qfalse;
	}

	LifetimeComponent *life = registry->try_get<LifetimeComponent>(enttEntity);
	if (!life) {
		life = &registry->emplace<LifetimeComponent>(enttEntity);
	}

	life->remaining = seconds;
	life->destroyOnExpire = qtrue;
	return qtrue;
}

/*
================
ECS_ClearLifetime
Remove the LifetimeComponent if present
================
*/
void ECS_ClearLifetime(ecs_entity_t entity) {
	entt::registry *registry = nullptr;
	entt::entity enttEntity = entt::null;
	if (!ECS_GetRegistryAndEntity(entity, &registry, &enttEntity)) {
		return;
	}

	if (registry->any_of<LifetimeComponent>(enttEntity)) {
		registry->remove<LifetimeComponent>(enttEntity);
	}
}

/*
================
ECS_GetRegistry
Get the EnTT registry pointer (for C++ code)
================
*/
ecs_registry_t *ECS_GetRegistry(void) {
	return reinterpret_cast<ecs_registry_t *>(g_registry);
}

// Helper functions for C++ code to access the registry
namespace ECS {
	entt::registry &GetRegistry() {
		assert(g_registry != nullptr);
		return *g_registry;
	}
	
	entt::entity GetEntityFromIndex(int index) {
		auto it = g_entityMap.find(index);
		if (it != g_entityMap.end()) {
			return it->second;
		}
		return entt::null;
	}
	
	int GetIndexFromEntity(entt::entity entity) {
		auto it = g_reverseEntityMap.find(entity);
		if (it != g_reverseEntityMap.end()) {
			return it->second;
		}
		return -1;
	}
	
	void MapEntity(int index, entt::entity entity) {
		g_entityMap[index] = entity;
		g_reverseEntityMap[entity] = index;
	}
	
	void UnmapEntity(int index) {
		auto it = g_entityMap.find(index);
		if (it != g_entityMap.end()) {
			g_reverseEntityMap.erase(it->second);
			g_entityMap.erase(it);
		}
	}
}

// C++ helper functions that can be called from other files
entt::entity ECS_GetEntityFromIndex(int index) {
	return ECS::GetEntityFromIndex(index);
}

int ECS_GetIndexFromEntity(entt::entity entity) {
	return ECS::GetIndexFromEntity(entity);
}

void ECS_MapEntity(int index, entt::entity entity) {
	ECS::MapEntity(index, entity);
}

void ECS_UnmapEntity(int index) {
	ECS::UnmapEntity(index);
}

#endif // USE_ENTT

