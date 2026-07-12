/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "server.h"
#include "sv_physics.h"
#include "engine_phys_map.h"
#include "../physics/phys_bullet.h"
#include "../physics/phys_props.h"
#include "../physics/phys_middleware.h"
#include "../physics/phys_character.h"
#include "../physics/phys_ragdoll_bind.h"
#include "../physics/phys_dmm.h"

static cvar_t *sv_physSpawn;
static cvar_t *sv_physEnabled;
static enginePhysMapList_t sv_physMapList;
static int sv_physSpawnCount;
static qboolean sv_physWorldReady;

static physBodyHandle_t SV_Physics_SpawnDef( const enginePhysMapDef_t *def ) {
	physBodyDef_t bd;
	physBodyHandle_t body = -1;

	if ( !def ) {
		return -1;
	}

	Com_Memset( &bd, 0, sizeof( bd ) );
	VectorCopy( def->origin, bd.position );
	VectorCopy( def->angles, bd.rotation );
	bd.materialId = def->materialId;
	bd.friction = 0.5f;
	bd.restitution = 0.2f;

	switch ( def->type ) {
	case ENGINE_PHYS_BOX:
		bd.shape = PHYS_SHAPE_BOX;
		bd.type = PHYS_BODY_DYNAMIC;
		bd.mass = def->mass > 0.0f ? def->mass : 20.0f;
		VectorCopy( def->halfExtents, bd.halfExtents );
		body = Phys_CreateBody( &bd );
		break;
	case ENGINE_PHYS_SPHERE:
		bd.shape = PHYS_SHAPE_SPHERE;
		bd.type = PHYS_BODY_DYNAMIC;
		bd.mass = def->mass > 0.0f ? def->mass : 15.0f;
		bd.radius = def->radius > 0.0f ? def->radius : 16.0f;
		body = Phys_CreateBody( &bd );
		break;
	case ENGINE_PHYS_STATIC:
		bd.shape = PHYS_SHAPE_BOX;
		bd.type = PHYS_BODY_STATIC;
		bd.mass = 0.0f;
		VectorCopy( def->halfExtents, bd.halfExtents );
		body = Phys_CreateBody( &bd );
		break;
	case ENGINE_PHYS_SENSOR:
		bd.shape = PHYS_SHAPE_BOX;
		bd.type = PHYS_BODY_STATIC;
		bd.isSensor = qtrue;
		bd.mass = 0.0f;
		VectorCopy( def->halfExtents, bd.halfExtents );
		body = Phys_CreateBody( &bd );
		break;
	case ENGINE_PHYS_SLIDER: {
		physBodyDef_t baseDef, slideDef;
		physConstraintDef_t jd;
		physBodyHandle_t base, slide;

		Com_Memset( &baseDef, 0, sizeof( baseDef ) );
		baseDef.shape = PHYS_SHAPE_BOX;
		baseDef.type = PHYS_BODY_STATIC;
		VectorCopy( def->origin, baseDef.position );
		VectorSet( baseDef.halfExtents, 8.0f, 8.0f, 8.0f );
		base = Phys_CreateBody( &baseDef );

		Com_Memset( &slideDef, 0, sizeof( slideDef ) );
		slideDef.shape = PHYS_SHAPE_BOX;
		slideDef.type = PHYS_BODY_DYNAMIC;
		slideDef.mass = def->mass > 0.0f ? def->mass : 20.0f;
		VectorSet( slideDef.position, def->origin[0], def->origin[1], def->origin[2] + 48.0f );
		VectorCopy( def->halfExtents, slideDef.halfExtents );
		slide = Phys_CreateBody( &slideDef );

		Com_Memset( &jd, 0, sizeof( jd ) );
		jd.type = PHYS_CONSTRAINT_SLIDER;
		jd.bodyA = base;
		jd.bodyB = slide;
		if ( def->axis[0] != 0.0f || def->axis[1] != 0.0f || def->axis[2] != 0.0f ) {
			VectorCopy( def->axis, jd.axisA );
		} else {
			VectorSet( jd.axisA, 0.0f, 0.0f, 1.0f );
		}
		jd.lowerLimit = def->sliderLower;
		jd.upperLimit = def->sliderUpper > 0.0f ? def->sliderUpper : 96.0f;
		jd.enableMotor = qtrue;
		jd.motorSpeed = 40.0f;
		jd.maxMotorForce = 8000.0f;
		jd.disableCollision = qtrue;
		Phys_CreateConstraint( &jd );
		body = slide;
		break;
	}
	case ENGINE_PHYS_RAGDOLL: {
		physBoundRagdoll_t bound;
		const char *model = def->model[0] ? def->model : NULL;
		if ( !Phys_RagdollSpawnBoundEx( model, def->origin, &bound, def->ragdollDead ) ) {
			return -1;
		}
		Com_Printf( "[physics] map misc_phys_ragdoll ragdoll=%d anim=%d motor=%d dead=%d\n",
			bound.ragdoll, bound.anim, bound.motor, def->ragdollDead ? 1 : 0 );
		body = 0;
		break;
	}
	case ENGINE_PHYS_DMM: {
		dmmObjectDef_t dd;
		dmmFracturePattern_t pattern;
		dmmObjectHandle_t h;
		float edge;

		Com_Memset( &dd, 0, sizeof( dd ) );
		VectorCopy( def->origin, dd.position );
		edge = def->halfExtents[0] * 2.0f;
		if ( edge < 8.0f ) {
			edge = 32.0f;
		}
		VectorSet( dd.dimensions, edge, edge, edge );
		if ( def->halfExtents[0] > 0.0f && def->halfExtents[1] > 0.0f && def->halfExtents[2] > 0.0f ) {
			dd.dimensions[0] = def->halfExtents[0] * 2.0f;
			dd.dimensions[1] = def->halfExtents[1] * 2.0f;
			dd.dimensions[2] = def->halfExtents[2] * 2.0f;
		}
		dd.material = DMM_CONCRETE;
		dd.density = def->mass > 0.0f ? def->mass * 0.05f : 2.4f;
		dd.gridResolution = 6;
		dd.deformability = 1.0f;
		dd.entityNum = -1;
		Dmm_GenerateVoronoiPattern( def->origin, edge * 0.5f, 8, &pattern );
		h = Dmm_CreateEnhanced( &dd, &pattern );
		if ( h < 0 ) {
			return -1;
		}
		Com_Printf( "[physics] map func_destructible/misc_phys_dmm handle=%d\n", h );
		body = 0;
		break;
	}
	default:
		break;
	}
	return body;
}

void SV_Physics_Init( void ) {
	sv_physEnabled = Cvar_Get( "phys_enabled", "1", CVAR_ARCHIVE );
	sv_physSpawn = Cvar_Get( "sv_physSpawn", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( sv_physSpawn,
		"Spawn misc_phys_* Soft Step props from the map entity string (server)." );
	Phys_CharacterInit();
	EnginePhysMap_Clear( &sv_physMapList );
	sv_physSpawnCount = 0;
	sv_physWorldReady = qfalse;
	Com_Printf( "[physics] server Soft Step hooks ready (sv_physSpawn=%d)\n",
		sv_physSpawn->integer );
}

void SV_Physics_Shutdown( void ) {
	SV_Physics_Clear();
	if ( sv_physWorldReady && com_dedicated && com_dedicated->integer ) {
		Phys_Shutdown();
		sv_physWorldReady = qfalse;
	}
}

void SV_Physics_Clear( void ) {
	EnginePhysMap_Clear( &sv_physMapList );
	sv_physSpawnCount = 0;
}

void SV_Physics_LoadMap( const char *entityString ) {
	SV_Physics_Clear();
	if ( !sv_physEnabled || !sv_physEnabled->integer ) {
		return;
	}
	if ( !Phys_Init() ) {
		Com_Printf( S_COLOR_YELLOW "[physics] server: Soft Step world unavailable\n" );
		return;
	}
	sv_physWorldReady = qtrue;
	Phys_LoadBSPCollision();
	EnginePhysMap_Parse( entityString, &sv_physMapList );
	if ( sv_physMapList.count > 0 ) {
		Com_Printf( "[physics] map parsed %d misc_phys_* entit(ies)\n", sv_physMapList.count );
	}
}

void SV_Physics_SpawnMapEntities( void ) {
	int i;

	sv_physSpawnCount = 0;
	if ( !sv_physSpawn || !sv_physSpawn->integer ) {
		return;
	}
	if ( !sv_physWorldReady ) {
		return;
	}
	if ( sv_physMapList.count <= 0 ) {
		return;
	}

	for ( i = 0; i < sv_physMapList.count; i++ ) {
		if ( SV_Physics_SpawnDef( &sv_physMapList.defs[i] ) >= 0 ) {
			sv_physSpawnCount++;
		}
	}
	if ( sv_physSpawnCount > 0 ) {
		Com_Printf( "[physics] spawned %d Soft Step map prop(s) (sv_physSpawn)\n",
			sv_physSpawnCount );
	}
}

void SV_Physics_Frame( int frameMsec ) {
	float dt;

	if ( !sv_physEnabled || !sv_physEnabled->integer ) {
		return;
	}
	/* Listen servers: client CL_GameFrame owns Soft Step tick to avoid double-step. */
	if ( !com_dedicated || !com_dedicated->integer ) {
		return;
	}
	if ( !sv_physWorldReady ) {
		if ( !Phys_Init() ) {
			return;
		}
		sv_physWorldReady = qtrue;
		Com_Printf( "[physics] server Soft Step world created (dedicated, phys_enabled)\n" );
	}
	dt = frameMsec * 0.001f;
	if ( dt <= 0.0f || dt > 0.1f ) {
		dt = 1.0f / 60.0f;
	}
	Phys_StepSimulation( dt );
	PhysMiddleware_Frame( dt );
}
