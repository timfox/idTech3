/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CBT-inspired GPU-driven terrain tessellation.
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
void CBTerrain_Frame( void );

void CBTerrain_Status_f( void );
void CBTerrain_Load_f( void );
void CBTerrain_Splat_f( void );

#endif /* VK_TERRAIN_H */
