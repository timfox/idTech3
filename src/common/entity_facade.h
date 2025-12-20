/*
===========================================================================
Entity Facade

Small C++23-style wrapper around the ECS C API to provide an OOP interface
for gameplay and tools without exposing entt directly.
===========================================================================
*/

#pragma once

#ifdef __cplusplus

#include <optional>
#include <string_view>

#include "ecs.h"

#ifdef USE_ENTT

class Entity {
public:
	Entity() = default;
	explicit Entity( ecs_entity_t id ) : id_( id ) {}

	// Factory helpers
	static std::optional<Entity> Create();
	static std::optional<Entity> FromIndex( int index );

	[[nodiscard]] ecs_entity_t id() const { return id_; }
	[[nodiscard]] bool valid() const;

	bool setTransform( const vec3_t position, const vec3_t rotation, const vec3_t scale );
	bool setPhysics( const vec3_t velocity, const vec3_t acceleration, float mass, float friction, qboolean useBullet );
	bool setHealth( int health, int maxHealth, int armor, int maxArmor );
	bool setLifetime( float seconds );
	void clearLifetime();

	// Map to legacy entity index for network syncing bridges.
	bool mapToIndex( int index );
	void unmapFromIndex( int index );

private:
	ecs_entity_t id_ = ECS_NULL_ENTITY;
};

#endif // USE_ENTT

#endif // __cplusplus


