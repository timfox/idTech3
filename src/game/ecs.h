/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

ECS (Entity-Component-System) API - alternative to legacy gentity.
Uses EnTT under the hood. Coexists with gentity; does not replace it.
===========================================================================
*/

#ifndef ECS_H
#define ECS_H

#include "../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ECS_INVALID_ENTITY  ((uint32_t)-1)
#define ECS_MAX_COMPONENT_NAME  48

/* Opaque handle; use ECS_EntityToId / ECS_IdToEntity for conversion */
typedef uint32_t ecs_entity_t;

/* Predefined component IDs for Lua/script access */
typedef enum {
	ECS_COMP_POSITION = 0,
	ECS_COMP_ROTATION,
	ECS_COMP_SCALE,
	ECS_COMP_VELOCITY,
	ECS_COMP_HEALTH,
	ECS_COMP_TAG,
	ECS_COMP_GENTITY_LINK,  /* link to legacy gentity number */
	ECS_COMP_COUNT
} ecs_component_id_t;

/* ---- Lifecycle ---- */
void ECS_Init( void );
void ECS_Shutdown( void );

/* ---- Entity ---- */
ecs_entity_t ECS_Create( void );
void         ECS_Destroy( ecs_entity_t e );
qboolean     ECS_Valid( ecs_entity_t e );
uint32_t     ECS_Count( void );

/* ---- Components (by ID) ---- */
qboolean ECS_Has( ecs_entity_t e, ecs_component_id_t comp );
void     ECS_Add( ecs_entity_t e, ecs_component_id_t comp );
void     ECS_Remove( ecs_entity_t e, ecs_component_id_t comp );

/* Position: x,y,z */
void ECS_SetPosition( ecs_entity_t e, float x, float y, float z );
void ECS_GetPosition( ecs_entity_t e, vec3_t out );

/* Rotation: pitch, yaw, roll (degrees) */
void ECS_SetRotation( ecs_entity_t e, float pitch, float yaw, float roll );
void ECS_GetRotation( ecs_entity_t e, vec3_t out );

/* Scale: x,y,z */
void ECS_SetScale( ecs_entity_t e, float x, float y, float z );
void ECS_GetScale( ecs_entity_t e, vec3_t out );

/* Velocity */
void ECS_SetVelocity( ecs_entity_t e, float x, float y, float z );
void ECS_GetVelocity( ecs_entity_t e, vec3_t out );

/* Health */
void   ECS_SetHealth( ecs_entity_t e, float value );
float  ECS_GetHealth( ecs_entity_t e );

/* Tag: string identifier */
void        ECS_SetTag( ecs_entity_t e, const char *tag );
const char *ECS_GetTag( ecs_entity_t e );

/* Gentity link: associate with legacy gentity number */
void ECS_SetGentityLink( ecs_entity_t e, int gentityNum );
int  ECS_GetGentityLink( ecs_entity_t e );

/* ---- Iteration (for systems) ---- */
typedef void (*ecs_iter_cb_t)( ecs_entity_t e, void *userdata );

/* Iterate entities that have all of the given components */
void ECS_Each( ecs_component_id_t *components, int count, ecs_iter_cb_t cb, void *userdata );

/* Integrate velocity into position (dt seconds). No-op if position/velocity missing. */
void ECS_StepMotion( float deltaTime );

/* ---- Component name lookup (for Lua) ---- */
ecs_component_id_t ECS_ComponentFromName( const char *name );
const char        *ECS_ComponentName( ecs_component_id_t id );

#ifdef __cplusplus
}
#endif

#endif /* ECS_H */
