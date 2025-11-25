/*
===========================================================================
ECS Core Systems

Core system implementations for physics, health, and network sync.
===========================================================================
*/

#ifdef USE_ENTT

#include "ecs.h"
#include "ecs_components.h"
#include "ecs_internal.h"
#include <entt/entt.hpp>

/*
================
PhysicsSystem
Update physics components (velocity, position)
================
*/
void ECS_PhysicsSystem_Update(float deltaTime) {
	entt::registry &registry = ECS::GetRegistry();
	
	auto view = registry.view<TransformComponent, PhysicsComponent>();
	
	for (auto entity : view) {
		auto &transform = view.get<TransformComponent>(entity);
		auto &physics = view.get<PhysicsComponent>(entity);
		
		// Update velocity from acceleration
		vec3_t deltaVel;
		VectorScale(physics.acceleration, deltaTime, deltaVel);
		VectorAdd(physics.velocity, deltaVel, physics.velocity);
		
		// Apply friction
		if (physics.friction > 0.0f) {
			float frictionFactor = 1.0f - (physics.friction * deltaTime);
			if (frictionFactor < 0.0f) frictionFactor = 0.0f;
			VectorScale(physics.velocity, frictionFactor, physics.velocity);
		}
		
		// Update position from velocity
		vec3_t deltaPos;
		VectorScale(physics.velocity, deltaTime, deltaPos);
		VectorAdd(transform.position, deltaPos, transform.position);
		
		// Clear acceleration (should be set each frame if needed)
		VectorClear(physics.acceleration);
	}
}

/*
================
HealthSystem
Handle health/damage logic
================
*/
void ECS_HealthSystem_Update(void) {
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return;
	entt::registry &registry = *registry_ptr;
	
	auto view = registry.view<HealthComponent>();
	
	for (auto entity : view) {
		auto &health = view.get<HealthComponent>(entity);
		
		// Clamp health values
		if (health.health > health.maxHealth) {
			health.health = health.maxHealth;
		}
		if (health.health < 0) {
			health.health = 0;
		}
		if (health.armor > health.maxArmor) {
			health.armor = health.maxArmor;
		}
		if (health.armor < 0) {
			health.armor = 0;
		}
	}
}

/*
================
NetworkSyncSystem
Sync ECS entities to engine entity structures for network
This is a placeholder - actual implementation will sync to gentity_t/svEntity_t
================
*/
void ECS_NetworkSyncSystem_Update(void) {
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return;
	entt::registry &registry = *registry_ptr;
	
	auto view = registry.view<NetworkComponent, TransformComponent>();
	
	for (auto entity : view) {
		auto &network = view.get<NetworkComponent>(entity);
		// TransformComponent is included in view but not used yet
		// It will be used when syncing to actual gentity_t/svEntity_t
		
		if (!network.needsSync || network.entityIndex < 0) {
			continue;
		}
		
		// This will be implemented to sync to actual gentity_t/svEntity_t
		// For now, just mark that sync is needed
		network.needsSync = qfalse;
	}
}

/*
================
ECS_RunFrame
Run all ECS systems for a frame
================
*/
void ECS_RunFrame(float deltaTime) {
	// Check if registry is initialized by checking if it exists
	extern ecs_registry_t *ECS_GetRegistry(void);
	if (ECS_GetRegistry() == nullptr) {
		return; // System not initialized
	}
	
	// Run systems in order
	ECS_PhysicsSystem_Update(deltaTime);
	ECS_HealthSystem_Update();
	ECS_NetworkSyncSystem_Update();
}

#endif // USE_ENTT

