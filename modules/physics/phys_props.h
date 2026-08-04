/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Gameplay props, clip boxes, and game-motion shadow bodies on top of Bullet.
Shadow bodies follow entity pose (game/Pmove motion) and can push free
rigid bodies without being pushed back.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"
#include "phys_bullet.h"

#define PHYS_PROP_MAX_SHADOWS   256
#define PHYS_PROP_MAX_DEMO      64

/* Collision groups (bitmasks for physBodyDef_t.collisionGroup / Mask) */
#define PHYS_GROUP_DEFAULT      1
#define PHYS_GROUP_STATIC       2
#define PHYS_GROUP_DYNAMIC      4
#define PHYS_GROUP_SHADOW       8
#define PHYS_GROUP_CLIP         16
#define PHYS_GROUP_TRIGGER      32
#define PHYS_MASK_ALL           (-1)
#define PHYS_MASK_SOLID         (PHYS_GROUP_STATIC | PHYS_GROUP_DYNAMIC | PHYS_GROUP_SHADOW | PHYS_GROUP_CLIP)

typedef int physShadowHandle_t;

typedef struct physShadowDef_s {
	int             entityNum;
	vec3_t          origin;
	vec3_t          angles;
	physShape_t     shape;          /* box or sphere preferred */
	vec3_t          halfExtents;
	float           radius;
	float           height;         /* capsule */
	qboolean        allowMovement;  /* if false, pose locked after create */
	qboolean        allowRotation;
	int             materialId;
} physShadowDef_t;

void                PhysProp_Init( void );
void                PhysProp_Shutdown( void );
void                PhysProp_Clear( void );
void                PhysProp_Frame( float dt );

/* Init modes: free dynamic, immovable static, game-motion shadow */
physBodyHandle_t    PhysProp_CreateDynamic( const physBodyDef_t *def );
physBodyHandle_t    PhysProp_CreateStatic( const physBodyDef_t *def );
physShadowHandle_t  PhysProp_CreateShadow( const physShadowDef_t *def );
void                PhysProp_DestroyShadow( physShadowHandle_t handle );

physBodyHandle_t    PhysProp_CreateBox( const vec3_t origin, const vec3_t halfExtents,
	physBodyType_t type, float mass, int materialId );
physBodyHandle_t    PhysProp_CreateSphere( const vec3_t origin, float radius,
	physBodyType_t type, float mass, int materialId );
physBodyHandle_t    PhysProp_CreateCapsule( const vec3_t origin, float radius, float height,
	physBodyType_t type, float mass, int materialId );
physBodyHandle_t    PhysProp_CreateFromAABB( const vec3_t mins, const vec3_t maxs,
	physBodyType_t type, float mass, int materialId );
physBodyHandle_t    PhysProp_CreateClipBox( const vec3_t origin, const vec3_t halfExtents );

void                PhysProp_SetShadowPose( physShadowHandle_t handle, const vec3_t origin, const vec3_t angles );
physBodyHandle_t    PhysProp_GetShadowBody( physShadowHandle_t handle );
int                 PhysProp_FindShadowByEntity( int entityNum );
void                PhysProp_SyncShadows( void );

int                 PhysProp_GetShadowCount( void );
int                 PhysProp_GetDemoBodyCount( void );

#ifdef __cplusplus
}
#endif
