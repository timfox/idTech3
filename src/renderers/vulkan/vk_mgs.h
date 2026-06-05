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
int R_MGS_EffectiveTier( void );
qboolean R_MGS_UploadGaussians( uint32_t count, const void *src, size_t srcStride );
void R_MGS_EnsurePipelines( void );
void R_MGS_MarkLoaded( const char *mapBaseName, uint32_t gaussianCount );

#endif /* VK_MGS_H */
