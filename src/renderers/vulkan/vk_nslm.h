/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Neural Six-way Lightmaps (experimental): volumetric fog/smoke/dust GI.
See docs/NEURAL_SIXWAY_LIGHTMAPS.md.
===========================================================================
*/

#ifndef VK_NSLM_H
#define VK_NSLM_H

#include "tr_local.h"

void R_NSLM_Init( void );
void R_NSLM_Shutdown( void );
void R_NSLM_OnMapLoad( const char *mapBaseName );
void vk_nslm_apply_to_froxels( uint32_t groups_x, uint32_t groups_y, uint32_t groups_z );
qboolean R_NSLM_Active( void );

#endif /* VK_NSLM_H */
