/*
===========================================================================
Entity Facade Implementation
===========================================================================
*/

#ifdef __cplusplus

#include "entity_facade.h"
#include "oop_services.h"
#include <entt/entt.hpp>

#ifdef USE_ENTT

std::optional<Entity> Entity::Create() {
	const ecs_entity_t id = ECS_CreateEntity();
	if ( id == ECS_NULL_ENTITY ) {
		Services::ServiceLocator::Logger().log( Services::LogLevel::Warn, "ECS_CreateEntity failed" );
		return std::nullopt;
	}
	return Entity{ id };
}

std::optional<Entity> Entity::FromIndex( int index ) {
#ifdef USE_ENTT
	entt::entity e = ECS_GetEntityFromIndex( index );
	if ( e == entt::null ) {
		return std::nullopt;
	}
	return Entity{ static_cast<ecs_entity_t>( e ) };
#else
	(void)index;
	return std::nullopt;
#endif
}

bool Entity::valid() const {
	return ECS_IsValid( id_ ) == qtrue;
}

bool Entity::setTransform( const vec3_t position, const vec3_t rotation, const vec3_t scale ) {
	return ECS_SetTransform( id_, position, rotation, scale ) == qtrue;
}

bool Entity::setPhysics( const vec3_t velocity, const vec3_t acceleration, float mass, float friction, qboolean useBullet ) {
	return ECS_SetPhysics( id_, velocity, acceleration, mass, friction, useBullet ) == qtrue;
}

bool Entity::setHealth( int health, int maxHealth, int armor, int maxArmor ) {
	return ECS_SetHealth( id_, health, maxHealth, armor, maxArmor ) == qtrue;
}

bool Entity::setLifetime( float seconds ) {
	return ECS_SetLifetime( id_, seconds ) == qtrue;
}

void Entity::clearLifetime() {
	ECS_ClearLifetime( id_ );
}

bool Entity::mapToIndex( int index ) {
#ifdef USE_ENTT
	if ( !valid() ) {
		return false;
	}
	ECS_MapEntity( index, static_cast<entt::entity>( id_ ) );
	return true;
#else
	(void)index;
	return false;
#endif
}

void Entity::unmapFromIndex( int index ) {
#ifdef USE_ENTT
	ECS_UnmapEntity( index );
#else
	(void)index;
#endif
}

#endif // USE_ENTT

#endif // __cplusplus


