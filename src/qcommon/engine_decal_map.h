/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Shared parser for misc_decal map entities.
===========================================================================
*/

#ifndef ENGINE_DECAL_MAP_H
#define ENGINE_DECAL_MAP_H

#include "q_shared.h"

typedef struct {
	vec3_t		origin;
	float		radius;
	float		pitch;
	float		yaw;
	float		fadeSec;
	char		shader[MAX_QPATH];
} engineDecalMapDef_t;

#define MAX_ENGINE_MAP_DECALS 512

typedef struct {
	engineDecalMapDef_t	defs[MAX_ENGINE_MAP_DECALS];
	int					count;
} engineDecalMapList_t;

void EngineDecalMap_Clear( engineDecalMapList_t *list );
void EngineDecalMap_Parse( const char *entityString, engineDecalMapList_t *list );

#endif /* ENGINE_DECAL_MAP_H */
