/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CBT-inspired GPU-driven terrain tessellation.
Compute shader selects patches by screen-space LOD; indirect draw renders.
===========================================================================
*/

#ifndef VK_TERRAIN_H
#define VK_TERRAIN_H

#include "../../qcommon/q_shared.h"

void CBTerrain_RegisterCvars( void );
qboolean CBTerrain_IsEnabled( void );
float CBTerrain_GetScale( void );
int CBTerrain_GetGridSize( void );

#endif /* VK_TERRAIN_H */
