/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CBT-inspired GPU-driven terrain tessellation.
Raster Ultra 1.14 primary outdoor representation: tiled heightfield mesh
with screen-space LOD, height-derived normals, and splat layering.
===========================================================================
*/

#ifndef VK_TERRAIN_H
#define VK_TERRAIN_H

#include "q_shared.h"

void CBTerrain_RegisterCvars( void );
qboolean CBTerrain_IsEnabled( void );
float CBTerrain_GetScale( void );
int CBTerrain_GetGridSize( void );
qboolean CBTerrain_HasSplat( void );

/* Metadata / residency — world routing uses these (no allocation without metadata). */
qboolean CBTerrain_HasMetadata( void );
qboolean CBTerrain_ResourcesReady( void );

void CBTerrain_Frame( void );
void CBTerrain_OnWorldLoad( void );
void CBTerrain_OnWorldUnload( void );
void CBTerrain_OnOriginRebase( void );

/* Height / normal queries for biomes and vegetation (world XZ → height + up). */
qboolean CBTerrain_SampleHeight( float worldX, float worldZ, float *outHeight );
qboolean CBTerrain_SampleNormal( float worldX, float worldZ, vec3_t outNormal );
float CBTerrain_SampleSlope( float worldX, float worldZ );

void CBTerrain_Status_f( void );
void CBTerrain_Load_f( void );
void CBTerrain_Splat_f( void );

#endif /* VK_TERRAIN_H */
