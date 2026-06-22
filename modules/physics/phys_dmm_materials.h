/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

DMM material library and prefab definitions.
Provides physically-accurate material presets, constraint-based
structural analysis, splintering/shattering behaviors, and
ready-to-use destructible object prefabs.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"
#include "phys_bullet.h"
#include "phys_dmm.h"

typedef enum {
	DMM_FRAC_VORONOI,
	DMM_FRAC_RADIAL,
	DMM_FRAC_SPLINTER,
	DMM_FRAC_SHATTER,
	DMM_FRAC_SLICE,
	DMM_FRAC_CRUMBLE,
	DMM_FRAC_TEAR,
	DMM_FRAC_PEEL
} dmmFractureMode_t;

typedef struct dmmMaterialDef_s {
	char                name[64];
	dmmMaterialType_t   type;
	float               density;
	float               youngsModulus;
	float               poissonsRatio;
	float               yieldStrength;
	float               ultimateStrength;
	float               fractureEnergy;
	float               hardness;
	float               elasticity;
	float               dampingRatio;
	dmmFractureMode_t   fractureMode;
	float               meltingPoint;
	float               softeningPoint;
	float               thermalConductivity;
	float               specificHeat;
	float               thermalExpansion;
	qboolean            isFlammable;
	float               ignitionTemp;
	float               burnRate;
	qboolean            isBrittle;
	qboolean            isDuctile;
	float               splinterAngle;
	float               grainDirection[3];
} dmmMaterialDef_t;

typedef struct dmmConstraint_s {
	int     nodeA;
	int     nodeB;
	float   restLength;
	float   stiffness;
	float   damping;
	float   breakForce;
	qboolean broken;
} dmmConstraint_t;

#define DMM_MAX_CONSTRAINTS 2048
#define DMM_MAX_NODES       1024

typedef struct dmmNode_s {
	vec3_t  position;
	vec3_t  velocity;
	vec3_t  force;
	float   mass;
	float   invMass;
	qboolean fixed;
	float   damage;
} dmmNode_t;

typedef struct dmmStructure_s {
	dmmNode_t       nodes[DMM_MAX_NODES];
	int             numNodes;
	dmmConstraint_t constraints[DMM_MAX_CONSTRAINTS];
	int             numConstraints;
	dmmMaterialDef_t material;
	vec3_t          gravity;
	float           timeAccum;
	int             solverIterations;
	qboolean        active;
} dmmStructure_t;

typedef struct dmmPrefab_s {
	char                name[64];
	dmmMaterialType_t   material;
	vec3_t              dimensions;
	int                 gridResolution;
	float               health;
	dmmFractureMode_t   fractureMode;
	int                 minFragments;
	int                 maxFragments;
	qboolean            hasPhysics;
	char                model[MAX_QPATH];
	char                fracturedModel[MAX_QPATH];
	char                debrisModel[MAX_QPATH];
	char                breakSound[MAX_QPATH];
	char                stressSound[MAX_QPATH];
} dmmPrefab_t;

void DmmMat_GetPreset(dmmMaterialType_t type, dmmMaterialDef_t *out);
const char *DmmMat_GetName(dmmMaterialType_t type);

dmmStructure_t *DmmStruct_Create(const dmmMaterialDef_t *material, int numNodes);
void            DmmStruct_Destroy(dmmStructure_t *s);
void            DmmStruct_AddNode(dmmStructure_t *s, const vec3_t pos, float mass, qboolean fixed);
void            DmmStruct_AddConstraint(dmmStructure_t *s, int a, int b, float stiffness, float breakForce);
void            DmmStruct_BuildGrid(dmmStructure_t *s, const vec3_t origin, const vec3_t size, int resX, int resY, int resZ);
void            DmmStruct_ApplyForce(dmmStructure_t *s, int nodeIdx, const vec3_t force);
void            DmmStruct_ApplyImpact(dmmStructure_t *s, const vec3_t point, const vec3_t dir, float energy, float radius);
void            DmmStruct_Solve(dmmStructure_t *s, float dt);
int             DmmStruct_GetBrokenConstraints(dmmStructure_t *s);
void            DmmStruct_GenerateFragments(dmmStructure_t *s, const vec3_t impactPoint, dmmFractureMode_t mode, dmmFracturePattern_t *pattern);

void DmmPrefab_WoodenDoor(dmmPrefab_t *out);
void DmmPrefab_GlassPane(dmmPrefab_t *out);
void DmmPrefab_MetalBarrel(dmmPrefab_t *out);
void DmmPrefab_ConcreteWall(dmmPrefab_t *out);
void DmmPrefab_IceBlock(dmmPrefab_t *out);
void DmmPrefab_WoodenCrate(dmmPrefab_t *out);
void DmmPrefab_MetalGrate(dmmPrefab_t *out);
void DmmPrefab_BrickWall(dmmPrefab_t *out);
void DmmPrefab_Railing(dmmPrefab_t *out);
void DmmPrefab_TreeTrunk(dmmPrefab_t *out);

#ifdef __cplusplus
}
#endif
