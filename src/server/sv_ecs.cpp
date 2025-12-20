/*
===========================================================================
Server-Side ECS Implementation

Server-side ECS integration with svEntity_t bridge.
===========================================================================
*/

#ifdef USE_ENTT

#include "sv_ecs.h"
#include "../common/ecs_components.h"
#include "../common/ecs_internal.h"
#include <entt/entt.hpp>

extern server_t sv;

#ifdef __cplusplus
extern "C" {
#endif

/*
================
SV_ECS_Init
Initialize server-side ECS
================
*/
void SV_ECS_Init(void) {
	ECS_Init();
}

/*
================
SV_ECS_Shutdown
Shutdown server-side ECS
================
*/
void SV_ECS_Shutdown(void) {
	ECS_Shutdown();
}

/*
================
SV_ECS_RegisterSvEntity
Create ECS entity for an svEntity_t
================
*/
ecs_entity_t SV_ECS_RegisterSvEntity(svEntity_t *ent) {
	if (ent == nullptr) {
		return ECS_NULL_ENTITY;
	}
	
	// Find the index of this svEntity_t
	int index = ent - sv.svEntities;
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
	NetworkComponent network(index, ent->baseline.eType, qtrue);
	ECS::GetRegistry().emplace<NetworkComponent>(entity, network);
	
	// Add TransformComponent from entity state
	TransformComponent transform;
	VectorCopy(ent->baseline.origin, transform.position);
	VectorCopy(ent->baseline.angles, transform.rotation);
	VectorSet(transform.scale, 1.0f, 1.0f, 1.0f);
	ECS::GetRegistry().emplace<TransformComponent>(entity, transform);
	
	// Map entity
	ECS::MapEntity(index, entity);
	
	return static_cast<ecs_entity_t>(entity);
}

/*
================
SV_ECS_UnregisterSvEntity
Remove ECS entity mapping for an svEntity_t
================
*/
void SV_ECS_UnregisterSvEntity(svEntity_t *ent) {
	if (ent == nullptr) {
		return;
	}
	
	int index = ent - sv.svEntities;
	if (index < 0 || index >= MAX_GENTITIES) {
		return;
	}
	
	entt::entity entity = ECS::GetEntityFromIndex(index);
	if (entity != entt::null) {
		ECS::GetRegistry().destroy(entity);
		ECS::UnmapEntity(index);
	}
}

/*
================
SV_ECS_GetEntityFromSvEntity
Get ECS entity from svEntity_t
================
*/
ecs_entity_t SV_ECS_GetEntityFromSvEntity(svEntity_t *ent) {
	if (ent == nullptr) {
		return ECS_NULL_ENTITY;
	}
	
	int index = ent - sv.svEntities;
	if (index < 0 || index >= MAX_GENTITIES) {
		return ECS_NULL_ENTITY;
	}
	
	entt::entity entity = ECS::GetEntityFromIndex(index);
	return static_cast<ecs_entity_t>(entity);
}

/*
================
SV_ECS_GetSvEntityFromEntity
Get svEntity_t from ECS entity
================
*/
svEntity_t *SV_ECS_GetSvEntityFromEntity(ecs_entity_t entity) {
	if (!ECS_IsValid(entity)) {
		return nullptr;
	}
	
	entt::entity enttEntity = static_cast<entt::entity>(entity);
	entt::registry &registry = ECS::GetRegistry();
	
	if (!registry.all_of<NetworkComponent>(enttEntity)) {
		return nullptr;
	}
	
	auto &network = registry.get<NetworkComponent>(enttEntity);
	if (network.entityIndex < 0 || network.entityIndex >= MAX_GENTITIES) {
		return nullptr;
	}
	
	return &sv.svEntities[network.entityIndex];
}

/*
================
SV_ECS_SyncToSvEntity
Sync ECS components to svEntity_t for network
================
*/
void SV_ECS_SyncToSvEntity(void) {
	entt::registry &registry = ECS::GetRegistry();
	
	auto view = registry.view<NetworkComponent, TransformComponent>();
	
	for (auto entity : view) {
		auto &network = view.get<NetworkComponent>(entity);
		auto &transform = view.get<TransformComponent>(entity);
		
		if (!network.isServer || network.entityIndex < 0 || network.entityIndex >= MAX_GENTITIES) {
			continue;
		}
		
		if (!network.needsSync) {
			continue;
		}
		
		svEntity_t *svEnt = &sv.svEntities[network.entityIndex];
		
		// Sync transform to entity state
		VectorCopy(transform.position, svEnt->baseline.origin);
		VectorCopy(transform.rotation, svEnt->baseline.angles);
		
		network.needsSync = qfalse;
	}
}

/*
================
SV_ECS_SyncFromSvEntity
Sync svEntity_t to ECS components
================
*/
void SV_ECS_SyncFromSvEntity(void) {
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return;
	entt::registry &registry = *registry_ptr;
	
	auto view = registry.view<NetworkComponent, TransformComponent>();
	
	for (auto entity : view) {
		auto &network = view.get<NetworkComponent>(entity);
		auto &transform = view.get<TransformComponent>(entity);
		
		if (!network.isServer || network.entityIndex < 0 || network.entityIndex >= MAX_GENTITIES) {
			continue;
		}
		
		svEntity_t *svEnt = &sv.svEntities[network.entityIndex];
		
		// Sync entity state to transform
		VectorCopy(svEnt->baseline.origin, transform.position);
		VectorCopy(svEnt->baseline.angles, transform.rotation);
	}
}

/*
================
SV_ECS_RunFrame
Run server-side ECS systems for a frame
================
*/
void SV_ECS_RunFrame(float deltaTime) {
	// Check if registry is initialized
	if (ECS_GetRegistry() == nullptr) {
		return; // System not initialized
	}
	
	// Sync from svEntity_t first
	SV_ECS_SyncFromSvEntity();
	
	// Run server-specific network sync system
	SV_ECS_NetworkSyncSystem_Update();
	
	// Run core ECS systems
	ECS_RunFrame(deltaTime);
	
	// Sync back to svEntity_t
	SV_ECS_SyncToSvEntity();
}

/*
================
SV_ECS_EnableBulletForEntity
Opt a server entity into Bullet-backed physics via its ECS PhysicsComponent.
Requires USE_ENTT and USE_BULLET.
================
*/
void SV_ECS_EnableBulletForEntity(svEntity_t *ent, float mass, float friction) {
	if (!ent) {
		return;
	}

#if defined(USE_ENTT) && defined(USE_BULLET)
	entt::registry &registry = ECS::GetRegistry();
	
	ecs_entity_t ecsEntity = SV_ECS_GetEntityFromSvEntity(ent);
	if (!ECS_IsValid(ecsEntity)) {
		ecsEntity = SV_ECS_RegisterSvEntity(ent);
		if (!ECS_IsValid(ecsEntity)) {
			return;
		}
	}
	
	entt::entity e = static_cast<entt::entity>(ecsEntity);
	
	// Ensure a PhysicsComponent exists
	if (!registry.all_of<PhysicsComponent>(e)) {
		registry.emplace<PhysicsComponent>(e);
	}
	
	auto &physics = registry.get<PhysicsComponent>(e);
	physics.mass = mass;
	physics.friction = friction;
	physics.useBullet = qtrue;
#else
	(void)ent;
	(void)mass;
	(void)friction;
#endif
}

/*
================
SV_ECS_DisableBulletForEntity
Disable Bullet-backed physics for a given server entity.
================
*/
void SV_ECS_DisableBulletForEntity(svEntity_t *ent) {
	if (!ent) {
		return;
	}

#if defined(USE_ENTT) && defined(USE_BULLET)
	ecs_entity_t ecsEntity = SV_ECS_GetEntityFromSvEntity(ent);
	if (!ECS_IsValid(ecsEntity)) {
		return;
	}
	
	entt::registry &registry = ECS::GetRegistry();
	entt::entity e = static_cast<entt::entity>(ecsEntity);
	
	if (!registry.all_of<PhysicsComponent>(e)) {
		return;
	}
	
	auto &physics = registry.get<PhysicsComponent>(e);
	physics.useBullet = qfalse;
#else
	(void)ent;
#endif
}

#ifdef __cplusplus
}
#endif

#endif // USE_ENTT

