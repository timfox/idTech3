/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Neural Irradiance Volume (experimental): compact 3D neural probe field decoded
from G-buffer depth/normals. See docs/NEURAL_IRRADIANCE_VOLUME.md.
===========================================================================
*/

#ifndef VK_NIV_H
#define VK_NIV_H

#include "tr_local.h"

void R_NIV_Init( void );
void R_NIV_Shutdown( void );
void R_NIV_OnMapLoad( const char *mapBaseName );
void vk_niv_apply_after_geometry( void );
qboolean R_NIV_Active( void );

#endif /* VK_NIV_H */
