/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

ECS implementation using EnTT. Alternative to gentity; coexists with it.
===========================================================================
*/

#include "ecs.h"
#include "../qcommon/q_shared.h"

#include <cstring>
#include <cctype>

static int strcasecmp_c( const char *a, const char *b ) {
	for ( ; *a && *b; a++, b++ ) {
		int ca = std::tolower( (unsigned char)*a );
		int cb = std::tolower( (unsigned char)*b );
		if ( ca != cb ) return ca - cb;
	}
	return std::tolower( (unsigned char)*a ) - std::tolower( (unsigned char)*b );
}

#include <entt/entt.hpp>

/* Component structs matching ecs_component_id_t */
struct PositionComponent {
	float x, y, z;
};

struct RotationComponent {
	float pitch, yaw, roll;
};

struct ScaleComponent {
	float x, y, z;
};

struct VelocityComponent {
	float x, y, z;
};

struct HealthComponent {
	float value;
};

struct TagComponent {
	char tag[ECS_MAX_COMPONENT_NAME];
};

struct GentityLinkComponent {
	int gentityNum;
};

static entt::registry *s_registry = nullptr;
static const char *s_compNames[ECS_COMP_COUNT] = {
	"position", "rotation", "scale", "velocity", "health", "tag", "gentity_link"
};

static entt::entity to_entt( ecs_entity_t e ) {
	return entt::entity{ e };
}

static ecs_entity_t from_entt( entt::entity e ) {
	return static_cast<ecs_entity_t>( entt::to_integral( e ) );
}

extern "C" {

void ECS_Init( void ) {
	if ( s_registry ) return;
	s_registry = new entt::registry();
}

void ECS_Shutdown( void ) {
	if ( !s_registry ) return;
	delete s_registry;
	s_registry = nullptr;
}

ecs_entity_t ECS_Create( void ) {
	if ( !s_registry ) return ECS_INVALID_ENTITY;
	return from_entt( s_registry->create() );
}

void ECS_Destroy( ecs_entity_t e ) {
	if ( !s_registry ) return;
	s_registry->destroy( to_entt( e ) );
}

qboolean ECS_Valid( ecs_entity_t e ) {
	if ( !s_registry ) return qfalse;
	return s_registry->valid( to_entt( e ) ) ? qtrue : qfalse;
}

uint32_t ECS_Count( void ) {
	if ( !s_registry ) return 0;
	return static_cast<uint32_t>( s_registry->storage<entt::entity>().size() );
}

static qboolean has_comp( ecs_entity_t e, ecs_component_id_t comp ) {
	if ( !s_registry ) return qfalse;
	entt::entity ent = to_entt( e );
	switch ( comp ) {
		case ECS_COMP_POSITION:     return s_registry->all_of<PositionComponent>( ent );
		case ECS_COMP_ROTATION:     return s_registry->all_of<RotationComponent>( ent );
		case ECS_COMP_SCALE:       return s_registry->all_of<ScaleComponent>( ent );
		case ECS_COMP_VELOCITY:    return s_registry->all_of<VelocityComponent>( ent );
		case ECS_COMP_HEALTH:      return s_registry->all_of<HealthComponent>( ent );
		case ECS_COMP_TAG:         return s_registry->all_of<TagComponent>( ent );
		case ECS_COMP_GENTITY_LINK: return s_registry->all_of<GentityLinkComponent>( ent );
		default: return qfalse;
	}
}

qboolean ECS_Has( ecs_entity_t e, ecs_component_id_t comp ) {
	return has_comp( e, comp );
}

void ECS_Add( ecs_entity_t e, ecs_component_id_t comp ) {
	if ( !s_registry ) return;
	entt::entity ent = to_entt( e );
	switch ( comp ) {
		case ECS_COMP_POSITION:     s_registry->emplace<PositionComponent>( ent, 0.f, 0.f, 0.f ); break;
		case ECS_COMP_ROTATION:     s_registry->emplace<RotationComponent>( ent, 0.f, 0.f, 0.f ); break;
		case ECS_COMP_SCALE:       s_registry->emplace<ScaleComponent>( ent, 1.f, 1.f, 1.f ); break;
		case ECS_COMP_VELOCITY:    s_registry->emplace<VelocityComponent>( ent, 0.f, 0.f, 0.f ); break;
		case ECS_COMP_HEALTH:      s_registry->emplace<HealthComponent>( ent, 100.f ); break;
		case ECS_COMP_TAG:         s_registry->emplace<TagComponent>( ent ); break;
		case ECS_COMP_GENTITY_LINK: s_registry->emplace<GentityLinkComponent>( ent, -1 ); break;
		default: break;
	}
}

void ECS_Remove( ecs_entity_t e, ecs_component_id_t comp ) {
	if ( !s_registry ) return;
	entt::entity ent = to_entt( e );
	switch ( comp ) {
		case ECS_COMP_POSITION:     s_registry->remove<PositionComponent>( ent ); break;
		case ECS_COMP_ROTATION:     s_registry->remove<RotationComponent>( ent ); break;
		case ECS_COMP_SCALE:       s_registry->remove<ScaleComponent>( ent ); break;
		case ECS_COMP_VELOCITY:    s_registry->remove<VelocityComponent>( ent ); break;
		case ECS_COMP_HEALTH:      s_registry->remove<HealthComponent>( ent ); break;
		case ECS_COMP_TAG:         s_registry->remove<TagComponent>( ent ); break;
		case ECS_COMP_GENTITY_LINK: s_registry->remove<GentityLinkComponent>( ent ); break;
		default: break;
	}
}

void ECS_SetPosition( ecs_entity_t e, float x, float y, float z ) {
	if ( !s_registry ) return;
	if ( !has_comp( e, ECS_COMP_POSITION ) ) ECS_Add( e, ECS_COMP_POSITION );
	auto &c = s_registry->get<PositionComponent>( to_entt( e ) );
	c.x = x; c.y = y; c.z = z;
}

void ECS_GetPosition( ecs_entity_t e, vec3_t out ) {
	VectorClear( out );
	if ( !s_registry || !has_comp( e, ECS_COMP_POSITION ) ) return;
	const auto &c = s_registry->get<PositionComponent>( to_entt( e ) );
	out[0] = c.x; out[1] = c.y; out[2] = c.z;
}

void ECS_SetRotation( ecs_entity_t e, float pitch, float yaw, float roll ) {
	if ( !s_registry ) return;
	if ( !has_comp( e, ECS_COMP_ROTATION ) ) ECS_Add( e, ECS_COMP_ROTATION );
	auto &c = s_registry->get<RotationComponent>( to_entt( e ) );
	c.pitch = pitch; c.yaw = yaw; c.roll = roll;
}

void ECS_GetRotation( ecs_entity_t e, vec3_t out ) {
	VectorClear( out );
	if ( !s_registry || !has_comp( e, ECS_COMP_ROTATION ) ) return;
	const auto &c = s_registry->get<RotationComponent>( to_entt( e ) );
	out[0] = c.pitch; out[1] = c.yaw; out[2] = c.roll;
}

void ECS_SetScale( ecs_entity_t e, float x, float y, float z ) {
	if ( !s_registry ) return;
	if ( !has_comp( e, ECS_COMP_SCALE ) ) ECS_Add( e, ECS_COMP_SCALE );
	auto &c = s_registry->get<ScaleComponent>( to_entt( e ) );
	c.x = x; c.y = y; c.z = z;
}

void ECS_GetScale( ecs_entity_t e, vec3_t out ) {
	VectorSet( out, 1.f, 1.f, 1.f );
	if ( !s_registry || !has_comp( e, ECS_COMP_SCALE ) ) return;
	const auto &c = s_registry->get<ScaleComponent>( to_entt( e ) );
	out[0] = c.x; out[1] = c.y; out[2] = c.z;
}

void ECS_SetVelocity( ecs_entity_t e, float x, float y, float z ) {
	if ( !s_registry ) return;
	if ( !has_comp( e, ECS_COMP_VELOCITY ) ) ECS_Add( e, ECS_COMP_VELOCITY );
	auto &c = s_registry->get<VelocityComponent>( to_entt( e ) );
	c.x = x; c.y = y; c.z = z;
}

void ECS_GetVelocity( ecs_entity_t e, vec3_t out ) {
	VectorClear( out );
	if ( !s_registry || !has_comp( e, ECS_COMP_VELOCITY ) ) return;
	const auto &c = s_registry->get<VelocityComponent>( to_entt( e ) );
	out[0] = c.x; out[1] = c.y; out[2] = c.z;
}

void ECS_SetHealth( ecs_entity_t e, float value ) {
	if ( !s_registry ) return;
	if ( !has_comp( e, ECS_COMP_HEALTH ) ) ECS_Add( e, ECS_COMP_HEALTH );
	s_registry->get<HealthComponent>( to_entt( e ) ).value = value;
}

float ECS_GetHealth( ecs_entity_t e ) {
	if ( !s_registry || !has_comp( e, ECS_COMP_HEALTH ) ) return 0.f;
	return s_registry->get<HealthComponent>( to_entt( e ) ).value;
}

void ECS_SetTag( ecs_entity_t e, const char *tag ) {
	if ( !s_registry ) return;
	if ( !has_comp( e, ECS_COMP_TAG ) ) ECS_Add( e, ECS_COMP_TAG );
	auto &c = s_registry->get<TagComponent>( to_entt( e ) );
	if ( tag ) {
		std::strncpy( c.tag, tag, sizeof( c.tag ) - 1 );
		c.tag[sizeof( c.tag ) - 1] = '\0';
	} else {
		c.tag[0] = '\0';
	}
}

const char *ECS_GetTag( ecs_entity_t e ) {
	if ( !s_registry || !has_comp( e, ECS_COMP_TAG ) ) return "";
	return s_registry->get<TagComponent>( to_entt( e ) ).tag;
}

void ECS_SetGentityLink( ecs_entity_t e, int gentityNum ) {
	if ( !s_registry ) return;
	if ( !has_comp( e, ECS_COMP_GENTITY_LINK ) ) ECS_Add( e, ECS_COMP_GENTITY_LINK );
	s_registry->get<GentityLinkComponent>( to_entt( e ) ).gentityNum = gentityNum;
}

int ECS_GetGentityLink( ecs_entity_t e ) {
	if ( !s_registry || !has_comp( e, ECS_COMP_GENTITY_LINK ) ) return -1;
	return s_registry->get<GentityLinkComponent>( to_entt( e ) ).gentityNum;
}

ecs_component_id_t ECS_ComponentFromName( const char *name ) {
	if ( !name ) return ECS_COMP_COUNT;
	for ( int i = 0; i < ECS_COMP_COUNT; i++ ) {
		if ( strcasecmp_c( name, s_compNames[i] ) == 0 )
			return (ecs_component_id_t)i;
	}
	return ECS_COMP_COUNT;
}

const char *ECS_ComponentName( ecs_component_id_t id ) {
	if ( id < 0 || id >= ECS_COMP_COUNT ) return "";
	return s_compNames[id];
}

/* ECS_Each: iterate entities with given components. */
void ECS_Each( ecs_component_id_t *components, int count, ecs_iter_cb_t cb, void *userdata ) {
	if ( !s_registry || !components || count <= 0 || !cb ) return;

	for ( auto &&[ent] : s_registry->storage<entt::entity>().each() ) {
		ecs_entity_t e = from_entt( ent );
		int i;
		for ( i = 0; i < count; i++ ) {
			if ( !has_comp( e, components[i] ) ) break;
		}
		if ( i == count )
			cb( e, userdata );
	}
}

void ECS_StepMotion( float deltaTime ) {
	if ( !s_registry || deltaTime <= 0.f ) return;
	auto view = s_registry->view<PositionComponent, VelocityComponent>();
	for ( auto ent : view ) {
		auto &p = s_registry->get<PositionComponent>( ent );
		const auto &v = s_registry->get<VelocityComponent>( ent );
		p.x += v.x * deltaTime;
		p.y += v.y * deltaTime;
		p.z += v.z * deltaTime;
	}
}

} /* extern "C" */
