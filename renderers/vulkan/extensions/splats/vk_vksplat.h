/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

VkSplat — Vulkan compute 3DGS training scaffold (Eurographics 2026).
See docs/VKSPLAT.md.
===========================================================================
*/

#ifndef VK_VKSPLAT_H
#define VK_VKSPLAT_H

#include "tr_local.h"

void R_VKSplat_Init( void );
void R_VKSplat_Shutdown( void );

qboolean R_VKSplat_Active( void );
qboolean R_VKSplat_RunTrainSteps( int steps );

#endif /* VK_VKSPLAT_H */
