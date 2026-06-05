/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "phys_materials.h"

static phys_material_t materials[PHYS_MAT_COUNT];
static qboolean matInitialized;

static void PhysMat_SetPreset( physMaterialId_t id, const char *name,
	float density, float friction, float restitution, float hardness,
	float brittleness, float ductility, float tearResistance, float wetness,
	float burnRate, float soundSharpness, float particleDustiness,
	float fractureThreshold, float stressAccumRate ) {
	phys_material_t *m;

	if ( id < 0 || id >= PHYS_MAT_COUNT ) {
		return;
	}
	m = &materials[id];
	Q_strncpyz( m->name, name, sizeof( m->name ) );
	m->density = density;
	m->friction = friction;
	m->restitution = restitution;
	m->hardness = hardness;
	m->brittleness = brittleness;
	m->ductility = ductility;
	m->tearResistance = tearResistance;
	m->wetness = wetness;
	m->burnRate = burnRate;
	m->soundSharpness = soundSharpness;
	m->particleDustiness = particleDustiness;
	m->fractureThreshold = fractureThreshold;
	m->stressAccumRate = stressAccumRate;
}

void PhysMat_Init( void ) {
	if ( matInitialized ) {
		return;
	}

	PhysMat_SetPreset( PHYS_MAT_DEFAULT, "default",
		1000.0f, 0.5f, 0.2f, 50.0f, 0.3f, 0.5f, 0.5f, 0.0f, 0.0f, 0.5f, 0.2f, 500.0f, 1.0f );
	PhysMat_SetPreset( PHYS_MAT_WOOD, "wood",
		600.0f, 0.55f, 0.15f, 30.0f, 0.6f, 0.3f, 0.4f, 0.2f, 0.05f, 0.4f, 0.5f, 120.0f, 1.2f );
	PhysMat_SetPreset( PHYS_MAT_GLASS, "glass",
		2500.0f, 0.4f, 0.05f, 80.0f, 0.95f, 0.05f, 0.1f, 0.0f, 0.0f, 0.95f, 0.1f, 40.0f, 2.5f );
	PhysMat_SetPreset( PHYS_MAT_METAL, "metal",
		7800.0f, 0.45f, 0.25f, 90.0f, 0.2f, 0.8f, 0.7f, 0.0f, 0.0f, 0.85f, 0.15f, 800.0f, 0.6f );
	PhysMat_SetPreset( PHYS_MAT_CONCRETE, "concrete",
		2400.0f, 0.7f, 0.1f, 70.0f, 0.75f, 0.15f, 0.2f, 0.1f, 0.0f, 0.6f, 0.85f, 200.0f, 1.5f );
	PhysMat_SetPreset( PHYS_MAT_STONE, "stone",
		2600.0f, 0.65f, 0.12f, 75.0f, 0.7f, 0.1f, 0.15f, 0.05f, 0.0f, 0.55f, 0.7f, 250.0f, 1.3f );
	PhysMat_SetPreset( PHYS_MAT_FLESH, "flesh",
		1050.0f, 0.6f, 0.05f, 10.0f, 0.15f, 0.9f, 0.8f, 0.6f, 0.0f, 0.2f, 0.3f, 60.0f, 0.8f );
	PhysMat_SetPreset( PHYS_MAT_MUD, "mud",
		1800.0f, 0.95f, 0.01f, 5.0f, 0.1f, 0.95f, 0.3f, 0.95f, 0.0f, 0.1f, 0.9f, 99999.0f, 0.2f );
	PhysMat_SetPreset( PHYS_MAT_WATER, "water",
		1000.0f, 0.01f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.05f, 0.0f, 99999.0f, 0.0f );
	PhysMat_SetPreset( PHYS_MAT_RUBBER, "rubber",
		1100.0f, 0.9f, 0.6f, 15.0f, 0.05f, 0.95f, 0.9f, 0.1f, 0.0f, 0.15f, 0.05f, 99999.0f, 0.1f );

	matInitialized = qtrue;
	Com_Printf( "PhysMat: %d gameplay materials registered\n", PHYS_MAT_COUNT );
}

const phys_material_t *PhysMat_Get( physMaterialId_t id ) {
	if ( !matInitialized ) {
		PhysMat_Init();
	}
	if ( id < 0 || id >= PHYS_MAT_COUNT ) {
		return &materials[PHYS_MAT_DEFAULT];
	}
	return &materials[id];
}

physMaterialId_t PhysMat_FindByName( const char *name ) {
	int i;

	if ( !name || !name[0] ) {
		return PHYS_MAT_DEFAULT;
	}
	if ( !matInitialized ) {
		PhysMat_Init();
	}
	for ( i = 0; i < PHYS_MAT_COUNT; i++ ) {
		if ( !Q_stricmp( materials[i].name, name ) ) {
			return i;
		}
	}
	return PHYS_MAT_DEFAULT;
}

void PhysMat_ApplyToBodyDef( physBodyDef_t *def, physMaterialId_t id ) {
	const phys_material_t *m;

	if ( !def ) {
		return;
	}
	m = PhysMat_Get( id );
	def->friction = m->friction;
	def->restitution = m->restitution;
	if ( def->mass <= 0.0f && m->density > 0.0f ) {
		float volume = def->halfExtents[0] * def->halfExtents[1] * def->halfExtents[2] * 8.0f;
		if ( volume < 1.0f ) {
			volume = 1.0f;
		}
		def->mass = volume * m->density * 0.001f;
	}
}

void PhysMat_ComputeImpactResponse( physMaterialId_t matA, physMaterialId_t matB,
	float impulseMag, float approachAngle, phys_impact_response_t *out ) {
	const phys_material_t *a;
	const phys_material_t *b;
	float hardnessMix;
	float brittlenessMix;

	if ( !out ) {
		return;
	}

	a = PhysMat_Get( matA );
	b = PhysMat_Get( matB );
	hardnessMix = ( a->hardness + b->hardness ) * 0.5f;
	brittlenessMix = ( a->brittleness + b->brittleness ) * 0.5f;

	Com_Memset( out, 0, sizeof( *out ) );
	out->damageScale = impulseMag * 0.001f * ( hardnessMix / 50.0f );
	out->particleScale = impulseMag * 0.0005f * ( a->particleDustiness + b->particleDustiness );
	out->soundScale = impulseMag * 0.0003f * ( a->soundSharpness + b->soundSharpness );
	out->decalScale = impulseMag * 0.0002f;
	out->stressAdd = impulseMag * a->stressAccumRate * b->stressAccumRate;
	out->shouldFracture = ( out->stressAdd > a->fractureThreshold * approachAngle ) ? qtrue : qfalse;
	out->shouldSplash = ( a->wetness > 0.5f || b->wetness > 0.5f ) ? qtrue : qfalse;
}

physMaterialId_t PhysMat_FromDmmType( dmmMaterialType_t dmmType ) {
	switch ( dmmType ) {
		case DMM_WOOD:        return PHYS_MAT_WOOD;
		case DMM_GLASS:       return PHYS_MAT_GLASS;
		case DMM_METAL_THIN:
		case DMM_METAL_THICK: return PHYS_MAT_METAL;
		case DMM_CONCRETE:    return PHYS_MAT_CONCRETE;
		case DMM_STONE:       return PHYS_MAT_STONE;
		case DMM_FLESH:       return PHYS_MAT_FLESH;
		case DMM_RUBBER:      return PHYS_MAT_RUBBER;
		default:              return PHYS_MAT_DEFAULT;
	}
}
