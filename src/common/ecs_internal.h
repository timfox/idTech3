/*
===========================================================================
ECS Internal Helper Functions

Internal helper functions for accessing the ECS registry from C++ code.
This header should only be included in .cpp files that need direct registry access.
===========================================================================
*/

#ifndef __ECS_INTERNAL_H__
#define __ECS_INTERNAL_H__

#ifdef USE_ENTT

#ifdef __cplusplus

#include "ecs.h"
#include <entt/entt.hpp>

// Helper functions to access ECS namespace functions
namespace ECS {
	entt::registry &GetRegistry();
	entt::entity GetEntityFromIndex(int index);
	int GetIndexFromEntity(entt::entity entity);
	void MapEntity(int index, entt::entity entity);
	void UnmapEntity(int index);
#ifdef USE_BULLET
	// Called before destroying an ECS entity to tear down Bullet state.
	void BulletOnEntityDestroyed(entt::registry &registry, entt::entity entity, struct PhysicsComponent &physics);
#endif
}

#endif // __cplusplus

#endif // USE_ENTT

#endif // __ECS_INTERNAL_H__

