/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Physical material table — drives Bullet friction/restitution, DMM fracture,
sound sharpness, particle dustiness, and impact response routing.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"
#include "phys_bullet.h"

typedef int physMaterialId_t;

#define PHYS_MAT_DEFAULT   0
#define PHYS_MAT_WOOD      1
#define PHYS_MAT_GLASS     2
#define PHYS_MAT_METAL     3
#define PHYS_MAT_CONCRETE  4
#define PHYS_MAT_STONE     5
#define PHYS_MAT_FLESH     6
#define PHYS_MAT_MUD       7
#define PHYS_MAT_WATER     8
#define PHYS_MAT_RUBBER    9
#define PHYS_MAT_COUNT     10

typedef struct phys_material_s {
	char    name[32];
	float   density;
	float   friction;
	float   restitution;
	float   hardness;
	float   brittleness;
	float   ductility;
	float   tearResistance;
	float   wetness;
	float   burnRate;
	float   soundSharpness;
	float   particleDustiness;
	float   fractureThreshold;
	float   stressAccumRate;
} phys_material_t;

typedef struct phys_impact_response_s {
	float   damageScale;
	float   particleScale;
	float   soundScale;
	float   decalScale;
	float   stressAdd;
	qboolean shouldFracture;
	qboolean shouldSplash;
} phys_impact_response_t;

void PhysMat_Init( void );
const phys_material_t *PhysMat_Get( physMaterialId_t id );
physMaterialId_t PhysMat_FindByName( const char *name );
void PhysMat_ApplyToBodyDef( physBodyDef_t *def, physMaterialId_t id );
void PhysMat_ComputeImpactResponse( physMaterialId_t matA, physMaterialId_t matB,
	float impulseMag, float approachAngle, phys_impact_response_t *out );
physMaterialId_t PhysMat_FromDmmType( dmmMaterialType_t dmmType );

#ifdef __cplusplus
}
#endif
