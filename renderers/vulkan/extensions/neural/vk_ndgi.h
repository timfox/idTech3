/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Neural Dynamic GI (experimental): temporal baked lightmaps from compressed
neural feature atlases + virtual-texture page decode. Vulkan renderer only.
===========================================================================
*/

#ifndef VK_NDGI_H
#define VK_NDGI_H

#include "tr_local.h"

void R_NDGI_Init( void );
void R_NDGI_Shutdown( void );
void R_NDGI_OnMapLoad( const char *mapBaseName );
void R_NDGI_FrameUpdate( void );
qboolean R_NDGI_Active( void );

#endif /* VK_NDGI_H */
