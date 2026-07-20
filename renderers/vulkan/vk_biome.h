/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Raster Ultra 1.14 — data-driven biome representation.
Deterministic seed + height/slope/moisture evaluation.
Does not regenerate placement differently every launch.
===========================================================================
*/

#ifndef VK_BIOME_H
#define VK_BIOME_H

#include "q_shared.h"

#define VK_BIOME_MAX_TYPES 16

typedef enum {
	VK_BIOME_SOIL = 0,
	VK_BIOME_GRASS,
	VK_BIOME_ROCK,
	VK_BIOME_SAND,
	VK_BIOME_MUD,
	VK_BIOME_SNOW,
	VK_BIOME_WETLAND,
	VK_BIOME_FOREST,
	VK_BIOME_DESERT,
	VK_BIOME_ASH,
	VK_BIOME_COUNT
} vkBiomeId_t;

typedef struct {
	vkBiomeId_t id;
	float elevMin, elevMax;
	float slopeMax;
	float moisture;
	float temperature;
	float vegDensity;
	float grassWeight;
	float treeWeight;
	float rockWeight;
	float snowWeight;
	float wetness;
	uint32_t layerMask; /* bit per terrain layer class */
} vkBiomeDef_t;

void VK_Biome_RegisterCvars( void );
void VK_Biome_Init( void );
void VK_Biome_Shutdown( void );
void VK_Biome_OnWorldLoad( void );
void VK_Biome_OnWorldUnload( void );
void VK_Biome_Frame( void );

qboolean VK_Biome_Active( void );
uint32_t VK_Biome_Seed( void );

/* Deterministic evaluation at world XZ (blended weights). */
void VK_Biome_Evaluate( float worldX, float worldZ, float weights[VK_BIOME_COUNT] );
vkBiomeId_t VK_Biome_Primary( float worldX, float worldZ );
float VK_Biome_VegetationDensity( float worldX, float worldZ );

const vkBiomeDef_t *VK_Biome_GetDef( vkBiomeId_t id );
void VK_Biome_Status_f( void );

#endif /* VK_BIOME_H */
