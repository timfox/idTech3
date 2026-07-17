/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Shared parser for misc_billboard / misc_flipbook / misc_imposter / misc_voxel map entities.
Used by the Vulkan renderer (client BSP), dedicated server (configstrings + spawn),
and regression tests.
===========================================================================
*/

#ifndef ENGINE_SPRITE_MAP_H
#define ENGINE_SPRITE_MAP_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	ENGINE_SPRITE_BILLBOARD = 0,
	ENGINE_SPRITE_FLIPBOOK,
	ENGINE_SPRITE_IMPOSTER,
	ENGINE_SPRITE_VOXEL
} engineSpriteType_t;

typedef struct {
	engineSpriteType_t	type;
	vec3_t				origin;
	float				radius;
	float				rotation;
	char				shader[MAX_QPATH];
	int					cols;
	int					rows;
	float				fps;
	float				swayAmount;
	float				swaySpeed;
} engineSpriteMapDef_t;

#define MAX_ENGINE_MAP_SPRITES 512

typedef struct {
	engineSpriteMapDef_t	defs[MAX_ENGINE_MAP_SPRITES];
	int						count;
} engineSpriteMapList_t;

void EngineSpriteMap_Clear( engineSpriteMapList_t *list );
void EngineSpriteMap_Parse( const char *entityString, engineSpriteMapList_t *list );

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_SPRITE_MAP_H */
