/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

WebSplatter — WebGPU-aligned tile compute Gaussian splatting (Vulkan path).
See docs/WEB_SPLATTER.md.
===========================================================================
*/

#ifndef VK_WSP_H
#define VK_WSP_H

#include "tr_local.h"

void R_WSP_Init( void );
void R_WSP_Shutdown( void );
void R_WSP_OnMapLoad( const char *mapBaseName );
void vk_wsp_apply_after_geometry( void );
qboolean R_WSP_Active( void );

#endif /* VK_WSP_H */
