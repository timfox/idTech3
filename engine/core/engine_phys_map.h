/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Shared parser for misc_phys_* map entities (Box3D Soft Step props).
===========================================================================
*/

#pragma once

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENGINE_PHYS_MAP_MAX 256

typedef enum {
	ENGINE_PHYS_BOX = 0,
	ENGINE_PHYS_SPHERE,
	ENGINE_PHYS_STATIC,
	ENGINE_PHYS_SENSOR,
	ENGINE_PHYS_SLIDER,
	ENGINE_PHYS_RAGDOLL
} enginePhysMapType_t;

typedef struct enginePhysMapDef_s {
	enginePhysMapType_t type;
	vec3_t              origin;
	vec3_t              angles;
	vec3_t              halfExtents;
	float               radius;
	float               mass;
	int                 materialId;
	vec3_t              axis;       /* slider slide axis (default +Z) */
	float               sliderLower;
	float               sliderUpper;
	char                targetname[64];
} enginePhysMapDef_t;

typedef struct enginePhysMapList_s {
	enginePhysMapDef_t defs[ENGINE_PHYS_MAP_MAX];
	int                count;
} enginePhysMapList_t;

void EnginePhysMap_Clear( enginePhysMapList_t *list );
void EnginePhysMap_Parse( const char *entityString, enginePhysMapList_t *list );

#ifdef __cplusplus
}
#endif
