/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Mobile-GS: tiered Vulkan compute Gaussian splatting for mobile-class GPUs.
See docs/MOBILE_GAUSSIAN_SPLATTING.md.
===========================================================================
*/

#ifndef VK_MGS_H
#define VK_MGS_H

#include "tr_local.h"

void R_MGS_Init( void );
void R_MGS_Shutdown( void );
void R_MGS_OnMapLoad( const char *mapBaseName );
void vk_mgs_apply_after_geometry( void );
qboolean R_MGS_Active( void );

#endif /* VK_MGS_H */
