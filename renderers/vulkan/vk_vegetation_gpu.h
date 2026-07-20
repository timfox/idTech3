/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Raster Ultra 1.14 — GPU-driven vegetation instance generation.
Deterministic blue-noise/hash placement; no per-blade CPU entity spam.
===========================================================================
*/

#ifndef VK_VEGETATION_GPU_H
#define VK_VEGETATION_GPU_H

#include "q_shared.h"

#define VK_VEG_MAX_INSTANCES     65536
#define VK_VEG_MAX_VISIBLE       8192
#define VK_VEG_MAX_SPECIES       16
#define VK_VEG_INTERACT_SIZE     64

typedef enum {
	VK_VEG_SPECIES_GRASS = 0,
	VK_VEG_SPECIES_FLOWER,
	VK_VEG_SPECIES_SHRUB,
	VK_VEG_SPECIES_BUSH,
	VK_VEG_SPECIES_REED,
	VK_VEG_SPECIES_FERN,
	VK_VEG_SPECIES_TREE_SMALL,
	VK_VEG_SPECIES_TREE_LARGE,
	VK_VEG_SPECIES_DEAD,
	VK_VEG_SPECIES_ROCK,
	VK_VEG_SPECIES_DEBRIS,
	VK_VEG_SPECIES_COUNT
} vkVegSpeciesId_t;

typedef struct {
	float pos[3];
	float rest[3];       /* undeformed spawn position */
	float scale;
	float yaw;
	float normal[3];
	uint32_t species;
	uint32_t biome;
	uint32_t lod;
	uint32_t flags; /* bit0 shadow, bit1 impostor, bit2 alpha */
	float windWeight;
	float prevPos[3]; /* motion vectors */
} vkVegInstance_t;

void VK_VegGpu_RegisterCvars( void );
void VK_VegGpu_Init( void );
void VK_VegGpu_Shutdown( void );
void VK_VegGpu_OnWorldLoad( void );
void VK_VegGpu_OnWorldUnload( void );
void VK_VegGpu_OnOriginRebase( void );
void VK_VegGpu_Frame( void );

qboolean VK_VegGpu_Active( void );
uint32_t VK_VegGpu_GeneratedCount( void );
uint32_t VK_VegGpu_VisibleCount( void );
uint32_t VK_VegGpu_RejectedCount( void );

void VK_VegGpu_Status_f( void );

#endif /* VK_VEGETATION_GPU_H */
