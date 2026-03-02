/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CBT-inspired GPU-driven terrain tessellation.
Cvars and init. Pipeline/buffers in vk.c.
===========================================================================
*/

#include "tr_local.h"
#include "vk_terrain.h"

static cvar_t *r_cbtTerrain;
static cvar_t *r_cbtTerrainScale;
static cvar_t *r_cbtTerrainGrid;

void CBTerrain_RegisterCvars( void ) {
	r_cbtTerrain = ri.Cvar_Get( "r_cbtTerrain", "0", CVAR_ARCHIVE );
	r_cbtTerrainScale = ri.Cvar_Get( "r_cbtTerrainScale", "256", CVAR_ARCHIVE );
	r_cbtTerrainGrid = ri.Cvar_Get( "r_cbtTerrainGrid", "32", CVAR_ARCHIVE );

	ri.Printf( PRINT_ALL, "CBT terrain tessellation: r_cbtTerrain %s (GPU-driven LOD)\n",
		r_cbtTerrain->integer ? "enabled" : "disabled" );
}

qboolean CBTerrain_IsEnabled( void ) {
	return r_cbtTerrain && r_cbtTerrain->integer > 0;
}

float CBTerrain_GetScale( void ) {
	return r_cbtTerrainScale ? r_cbtTerrainScale->value : 256.0f;
}

int CBTerrain_GetGridSize( void ) {
	int g = r_cbtTerrainGrid ? r_cbtTerrainGrid->integer : 32;
	if ( g < 2 ) g = 2;
	if ( g > 256 ) g = 256;
	return g;
}
