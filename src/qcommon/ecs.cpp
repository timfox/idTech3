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

