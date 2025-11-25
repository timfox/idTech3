/*
===========================================================================
Game Module ECS Implementation

Game module ECS integration with gentity_t bridge.
===========================================================================
*/

#ifdef USE_ENTT

#include "g_ecs.h"
#include "../../../../src/qcommon/ecs_components.h"
#include "../../../../src/qcommon/ecs.cpp" // For ECS namespace access
#include <entt/entt.hpp>

extern gentity_t g_entities[MAX_GENTITIES];
extern level_locals_t level;

/*
================
G_ECS_Init
Initialize game module ECS
================
*/
void G_ECS_Init(void) {
	ECS_Init();
}

/*
================
G_ECS_Shutdown
Shutdown game module ECS
================
*/
void G_ECS_Shutdown(void) {
	ECS_Shutdown();
}

/*
================
G_ECS_RegisterGentity
Create ECS entity for a gentity_t
================
*/
ecs_entity_t G_ECS_RegisterGentity(gentity_t *ent) {
	if (ent == nullptr) {
		return ECS_NULL_ENTITY;
	}
	
	// Find the index of this gentity_t
	int index = ent - g_entities;
	if (index < 0 || index >= MAX_GENTITIES) {
		return ECS_NULL_ENTITY;
	}
	
	// Check if already registered
	entt::entity existing = ECS::GetEntityFromIndex(index);
	if (existing != entt::null) {
		return static_cast<ecs_entity_t>(existing);
	}
	
	// Create new ECS entity
	entt::entity entity = ECS::GetRegistry().create();
	
	// Add NetworkComponent
	NetworkComponent network(index, ent->s.eType, qfalse);
	registry.emplace<NetworkComponent>(entity, network);
	
	// Add TransformComponent from entity state
	TransformComponent transform;
	VectorCopy(ent->s.origin, transform.position);
	VectorCopy(ent->s.angles, transform.rotation);
	VectorSet(transform.scale, 1.0f, 1.0f, 1.0f);
	registry.emplace<TransformComponent>(entity, transform);
	
	// Add HealthComponent if entity has health
	if (ent->health > 0) {
		HealthComponent health(ent->health, ent->health);
		registry.emplace<HealthComponent>(entity, health);
	}
	
	return static_cast<ecs_entity_t>(entity);
}

/*
================
G_ECS_UnregisterGentity
Remove ECS entity mapping for a gentity_t
================
*/
void G_ECS_UnregisterGentity(gentity_t *ent) {
	if (ent == nullptr) {
		return;
	}
	
	int index = ent - g_entities;
	if (index < 0 || index >= MAX_GENTITIES) {
		return;
	}
	
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return;
	entt::registry &registry = *registry_ptr;
	
	// Find entity by NetworkComponent
	auto view = registry.view<NetworkComponent>();
	for (auto ent : view) {
		auto &net = view.get<NetworkComponent>(ent);
		if (net.entityIndex == index && !net.isServer) {
			registry.destroy(ent);
			break;
		}
	}
}

/*
================
G_ECS_GetEntityFromGentity
Get ECS entity from gentity_t
================
*/
ecs_entity_t G_ECS_GetEntityFromGentity(gentity_t *ent) {
	if (ent == nullptr) {
		return ECS_NULL_ENTITY;
	}
	
	int index = ent - g_entities;
	if (index < 0 || index >= MAX_GENTITIES) {
		return ECS_NULL_ENTITY;
	}
	
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return ECS_NULL_ENTITY;
	entt::registry &registry = *registry_ptr;
	
	// Find entity by NetworkComponent
	auto view = registry.view<NetworkComponent>();
	for (auto ent : view) {
		auto &net = view.get<NetworkComponent>(ent);
		if (net.entityIndex == index && !net.isServer) {
			return static_cast<ecs_entity_t>(ent);
		}
	}
	return ECS_NULL_ENTITY;
}

/*
================
G_ECS_GetGentityFromEntity
Get gentity_t from ECS entity
================
*/
gentity_t *G_ECS_GetGentityFromEntity(ecs_entity_t entity) {
	if (!ECS_IsValid(entity)) {
		return nullptr;
	}
	
	entt::entity enttEntity = static_cast<entt::entity>(entity);
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return nullptr;
	entt::registry &registry = *registry_ptr;
	
	if (!registry.all_of<NetworkComponent>(enttEntity)) {
		return nullptr;
	}
	
	auto &network = registry.get<NetworkComponent>(enttEntity);
	if (network.entityIndex < 0 || network.entityIndex >= MAX_GENTITIES) {
		return nullptr;
	}
	
	return &g_entities[network.entityIndex];
}

/*
================
G_ECS_SyncToGentity
Sync ECS components to gentity_t for network
================
*/
void G_ECS_SyncToGentity(void) {
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return;
	entt::registry &registry = *registry_ptr;
	
	auto view = registry.view<NetworkComponent, TransformComponent>();
	
	for (auto entity : view) {
		auto &network = view.get<NetworkComponent>(entity);
		auto &transform = view.get<TransformComponent>(entity);
		
		if (network.isServer || network.entityIndex < 0 || network.entityIndex >= MAX_GENTITIES) {
			continue;
		}
		
		if (!network.needsSync) {
			continue;
		}
		
		gentity_t *gent = &g_entities[network.entityIndex];
		
		// Sync transform to entity state
		VectorCopy(transform.position, gent->s.origin);
		VectorCopy(transform.rotation, gent->s.angles);
		
		// Sync health if component exists
		if (registry.all_of<HealthComponent>(entity)) {
			auto &health = registry.get<HealthComponent>(entity);
			gent->health = health.health;
		}
		
		network.needsSync = qfalse;
	}
}

/*
================
G_ECS_SyncFromGentity
Sync gentity_t to ECS components
================
*/
void G_ECS_SyncFromGentity(void) {
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return;
	entt::registry &registry = *registry_ptr;
	
	auto view = registry.view<NetworkComponent, TransformComponent>();
	
	for (auto entity : view) {
		auto &network = view.get<NetworkComponent>(entity);
		auto &transform = view.get<TransformComponent>(entity);
		
		if (network.isServer || network.entityIndex < 0 || network.entityIndex >= MAX_GENTITIES) {
			continue;
		}
		
		gentity_t *gent = &g_entities[network.entityIndex];
		
		// Sync entity state to transform
		VectorCopy(gent->s.origin, transform.position);
		VectorCopy(gent->s.angles, transform.rotation);
		
		// Sync health if component exists
		if (registry.all_of<HealthComponent>(entity)) {
			auto &health = registry.get<HealthComponent>(entity);
			health.health = gent->health;
		}
	}
}

/*
================
G_ECS_RunFrame
Run game module ECS systems for a frame
================
*/
void G_ECS_RunFrame(float deltaTime) {
	// Check if registry is initialized
	if (ECS_GetRegistry() == nullptr) {
		return; // System not initialized
	}
	
	// Sync from gentity_t first
	G_ECS_SyncFromGentity();
	
	// Run core ECS systems
	ECS_RunFrame(deltaTime);
	
	// Run mod-specific systems
	extern void G_ECS_ModSystems_RunFrame(void);
	G_ECS_ModSystems_RunFrame();
	
	// Sync back to gentity_t
	G_ECS_SyncToGentity();
}

#endif // USE_ENTT

