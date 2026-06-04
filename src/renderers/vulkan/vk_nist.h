/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Neural Image Space Tessellation (experimental): G-buffer silhouette refinement.
See docs/NEURAL_IMAGE_SPACE_TESSELLATION.md.
===========================================================================
*/

#ifndef VK_NIST_H
#define VK_NIST_H

#include "tr_local.h"

void R_NIST_Init( void );
void R_NIST_Shutdown( void );
void R_NIST_OnMapLoad( const char *mapBaseName );
void vk_nist_apply_after_geometry( void );
qboolean R_NIST_Active( void );

#endif /* VK_NIST_H */
