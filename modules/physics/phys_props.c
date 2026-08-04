/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "phys_bullet.h"
#include "phys_materials.h"
#include "phys_props.h"

typedef struct physShadowSlot_s {
	qboolean         active;
	physBodyHandle_t body;
	int              entityNum;
	vec3_t           origin;
	vec3_t           angles;
	qboolean         allowMovement;
	qboolean         allowRotation;
} physShadowSlot_t;

static physShadowSlot_t shadows[PHYS_PROP_MAX_SHADOWS];
static int shadowCount;
static qboolean propsReady;

static void PhysProp_FillDefaults( physBodyDef_t *def, physBodyType_t type, float mass, int materialId ) {
	Com_Memset( def, 0, sizeof( *def ) );
	def->type = type;
	def->mass = ( type == PHYS_BODY_STATIC ) ? 0.0f : ( mass > 0.0f ? mass : 10.0f );
	def->friction = 0.5f;
	def->restitution = 0.2f;
	def->linearDamping = 0.05f;
	def->angularDamping = 0.05f;
	def->materialId = materialId;
	if ( type == PHYS_BODY_STATIC ) {
		def->collisionGroup = PHYS_GROUP_STATIC;
		def->collisionMask = PHYS_MASK_SOLID;
	} else if ( type == PHYS_BODY_KINEMATIC ) {
		def->collisionGroup = PHYS_GROUP_SHADOW;
		def->collisionMask = PHYS_MASK_SOLID;
	} else {
		def->collisionGroup = PHYS_GROUP_DYNAMIC;
		def->collisionMask = PHYS_MASK_SOLID;
	}
	if ( materialId > 0 ) {
		PhysMat_ApplyToBodyDef( def, materialId );
	}
}

void PhysProp_Init( void ) {
	if ( propsReady ) {
		return;
	}
	Com_Memset( shadows, 0, sizeof( shadows ) );
	shadowCount = 0;
	propsReady = qtrue;
	Com_Printf( "PhysProp: dynamic / static / shadow prop layer ready\n" );
}

void PhysProp_Shutdown( void ) {
	PhysProp_Clear();
	propsReady = qfalse;
}

void PhysProp_Clear( void ) {
	int i;

	for ( i = 0; i < PHYS_PROP_MAX_SHADOWS; i++ ) {
		if ( shadows[i].active && shadows[i].body >= 0 ) {
			Phys_DestroyBody( shadows[i].body );
		}
		Com_Memset( &shadows[i], 0, sizeof( shadows[i] ) );
		shadows[i].body = -1;
	}
	shadowCount = 0;
}

void PhysProp_Frame( float dt ) {
	(void)dt;
	if ( !propsReady ) {
		return;
	}
	PhysProp_SyncShadows();
}

physBodyHandle_t PhysProp_CreateDynamic( const physBodyDef_t *def ) {
	physBodyDef_t local;

	if ( !def ) {
		return -1;
	}
	if ( !propsReady ) {
		PhysProp_Init();
	}
	local = *def;
	local.type = PHYS_BODY_DYNAMIC;
	if ( !local.collisionGroup ) {
		local.collisionGroup = PHYS_GROUP_DYNAMIC;
	}
	if ( !local.collisionMask ) {
		local.collisionMask = PHYS_MASK_SOLID;
	}
	return Phys_CreateBody( &local );
}

physBodyHandle_t PhysProp_CreateStatic( const physBodyDef_t *def ) {
	physBodyDef_t local;

	if ( !def ) {
		return -1;
	}
	if ( !propsReady ) {
		PhysProp_Init();
	}
	local = *def;
	local.type = PHYS_BODY_STATIC;
	local.mass = 0.0f;
	if ( !local.collisionGroup ) {
		local.collisionGroup = PHYS_GROUP_STATIC;
	}
	if ( !local.collisionMask ) {
		local.collisionMask = PHYS_MASK_SOLID;
	}
	return Phys_CreateBody( &local );
}

physBodyHandle_t PhysProp_CreateBox( const vec3_t origin, const vec3_t halfExtents,
	physBodyType_t type, float mass, int materialId ) {
	physBodyDef_t def;

	PhysProp_FillDefaults( &def, type, mass, materialId );
	def.shape = PHYS_SHAPE_BOX;
	VectorCopy( origin, def.position );
	VectorCopy( halfExtents, def.halfExtents );
	if ( type == PHYS_BODY_STATIC ) {
		return PhysProp_CreateStatic( &def );
	}
	return PhysProp_CreateDynamic( &def );
}

physBodyHandle_t PhysProp_CreateSphere( const vec3_t origin, float radius,
	physBodyType_t type, float mass, int materialId ) {
	physBodyDef_t def;

	PhysProp_FillDefaults( &def, type, mass, materialId );
	def.shape = PHYS_SHAPE_SPHERE;
	VectorCopy( origin, def.position );
	def.radius = radius > 0.0f ? radius : 8.0f;
	if ( type == PHYS_BODY_STATIC ) {
		return PhysProp_CreateStatic( &def );
	}
	return PhysProp_CreateDynamic( &def );
}

physBodyHandle_t PhysProp_CreateCapsule( const vec3_t origin, float radius, float height,
	physBodyType_t type, float mass, int materialId ) {
	physBodyDef_t def;

	PhysProp_FillDefaults( &def, type, mass, materialId );
	def.shape = PHYS_SHAPE_CAPSULE;
	VectorCopy( origin, def.position );
	def.radius = radius > 0.0f ? radius : 8.0f;
	def.height = height > 0.0f ? height : 32.0f;
	if ( type == PHYS_BODY_STATIC ) {
		return PhysProp_CreateStatic( &def );
	}
	return PhysProp_CreateDynamic( &def );
}

physBodyHandle_t PhysProp_CreateFromAABB( const vec3_t mins, const vec3_t maxs,
	physBodyType_t type, float mass, int materialId ) {
	vec3_t origin, halfExtents;

	if ( !mins || !maxs ) {
		return -1;
	}
	origin[0] = ( mins[0] + maxs[0] ) * 0.5f;
	origin[1] = ( mins[1] + maxs[1] ) * 0.5f;
	origin[2] = ( mins[2] + maxs[2] ) * 0.5f;
	halfExtents[0] = ( maxs[0] - mins[0] ) * 0.5f;
	halfExtents[1] = ( maxs[1] - mins[1] ) * 0.5f;
	halfExtents[2] = ( maxs[2] - mins[2] ) * 0.5f;
	if ( halfExtents[0] < 0.5f ) {
		halfExtents[0] = 0.5f;
	}
	if ( halfExtents[1] < 0.5f ) {
		halfExtents[1] = 0.5f;
	}
	if ( halfExtents[2] < 0.5f ) {
		halfExtents[2] = 0.5f;
	}
	return PhysProp_CreateBox( origin, halfExtents, type, mass, materialId );
}

physBodyHandle_t PhysProp_CreateClipBox( const vec3_t origin, const vec3_t halfExtents ) {
	physBodyDef_t def;

	PhysProp_FillDefaults( &def, PHYS_BODY_STATIC, 0.0f, PHYS_MAT_DEFAULT );
	def.shape = PHYS_SHAPE_BOX;
	VectorCopy( origin, def.position );
	VectorCopy( halfExtents, def.halfExtents );
	def.collisionGroup = PHYS_GROUP_CLIP;
	/* Physics-only clip: collide with free bodies and shadows, not triggers */
	def.collisionMask = PHYS_GROUP_DYNAMIC | PHYS_GROUP_SHADOW;
	return PhysProp_CreateStatic( &def );
}

physShadowHandle_t PhysProp_CreateShadow( const physShadowDef_t *def ) {
	physBodyDef_t bodyDef;
	physShadowSlot_t *slot = NULL;
	int i;
	int idx = -1;

	if ( !def ) {
		return -1;
	}
	if ( !propsReady ) {
		PhysProp_Init();
	}

	for ( i = 0; i < PHYS_PROP_MAX_SHADOWS; i++ ) {
		if ( !shadows[i].active ) {
			idx = i;
			slot = &shadows[i];
			break;
		}
	}
	if ( !slot ) {
		Com_Printf( S_COLOR_YELLOW "PhysProp: no free shadow slots\n" );
		return -1;
	}

	PhysProp_FillDefaults( &bodyDef, PHYS_BODY_KINEMATIC, 0.0f, def->materialId );
	bodyDef.shape = def->shape;
	if ( bodyDef.shape != PHYS_SHAPE_BOX && bodyDef.shape != PHYS_SHAPE_SPHERE
		&& bodyDef.shape != PHYS_SHAPE_CAPSULE ) {
		bodyDef.shape = PHYS_SHAPE_BOX;
	}
	VectorCopy( def->origin, bodyDef.position );
	VectorCopy( def->angles, bodyDef.rotation );
	VectorCopy( def->halfExtents, bodyDef.halfExtents );
	bodyDef.radius = def->radius;
	bodyDef.height = def->height;
	bodyDef.collisionGroup = PHYS_GROUP_SHADOW;
	bodyDef.collisionMask = PHYS_MASK_SOLID;

	slot->body = Phys_CreateBody( &bodyDef );
	if ( slot->body < 0 ) {
		return -1;
	}

	slot->active = qtrue;
	slot->entityNum = def->entityNum;
	VectorCopy( def->origin, slot->origin );
	VectorCopy( def->angles, slot->angles );
	slot->allowMovement = def->allowMovement;
	slot->allowRotation = def->allowRotation;
	if ( idx >= shadowCount ) {
		shadowCount = idx + 1;
	}
	return idx;
}

void PhysProp_DestroyShadow( physShadowHandle_t handle ) {
	if ( handle < 0 || handle >= PHYS_PROP_MAX_SHADOWS || !shadows[handle].active ) {
		return;
	}
	if ( shadows[handle].body >= 0 ) {
		Phys_DestroyBody( shadows[handle].body );
	}
	Com_Memset( &shadows[handle], 0, sizeof( shadows[handle] ) );
	shadows[handle].body = -1;
}

void PhysProp_SetShadowPose( physShadowHandle_t handle, const vec3_t origin, const vec3_t angles ) {
	physShadowSlot_t *slot;

	if ( handle < 0 || handle >= PHYS_PROP_MAX_SHADOWS || !shadows[handle].active ) {
		return;
	}
	slot = &shadows[handle];
	if ( slot->allowMovement && origin ) {
		VectorCopy( origin, slot->origin );
	}
	if ( slot->allowRotation && angles ) {
		VectorCopy( angles, slot->angles );
	}
}

physBodyHandle_t PhysProp_GetShadowBody( physShadowHandle_t handle ) {
	if ( handle < 0 || handle >= PHYS_PROP_MAX_SHADOWS || !shadows[handle].active ) {
		return -1;
	}
	return shadows[handle].body;
}

int PhysProp_FindShadowByEntity( int entityNum ) {
	int i;

	for ( i = 0; i < PHYS_PROP_MAX_SHADOWS; i++ ) {
		if ( shadows[i].active && shadows[i].entityNum == entityNum ) {
			return i;
		}
	}
	return -1;
}

void PhysProp_SyncShadows( void ) {
	int i;

	for ( i = 0; i < PHYS_PROP_MAX_SHADOWS; i++ ) {
		if ( !shadows[i].active || shadows[i].body < 0 ) {
			continue;
		}
		Phys_SetBodyTargetTransform( shadows[i].body, shadows[i].origin, shadows[i].angles, 1.0f / 60.0f );
	}
}

int PhysProp_GetShadowCount( void ) {
	int i;
	int n = 0;

	for ( i = 0; i < PHYS_PROP_MAX_SHADOWS; i++ ) {
		if ( shadows[i].active ) {
			n++;
		}
	}
	return n;
}

int PhysProp_GetDemoBodyCount( void ) {
	return Phys_GetBodyCount();
}
