/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

SqueezeMe — mobile-ready distilled Gaussian full-body avatars (Vulkan).
See docs/SQUEEZEME.md and arXiv:2412.15171v4.
===========================================================================
*/

#ifndef VK_SQUEEZEME_H
#define VK_SQUEEZEME_H

#include "tr_local.h"

void R_SQZ_Init( void );
void R_SQZ_Shutdown( void );
void R_SQZ_OnMapLoad( const char *mapBaseName );
void R_SQZ_FrameUpdate( void );
void vk_sqz_apply_after_geometry( void );
qboolean R_SQZ_Active( void );
qboolean R_SQZ_Enabled( void );
int R_SQZ_EffectiveMgsTier( void );

#endif /* VK_SQUEEZEME_H */
