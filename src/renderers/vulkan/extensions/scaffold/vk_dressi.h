/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Dressi — hardware-agnostic differentiable renderer scaffold (Takimoto et al.
Eurographics 2022): HardSoftRas forward path, inverse UV, Vulkan stage chain.
See docs/DRESSI.md.
===========================================================================
*/

#ifndef VK_DRESSI_H
#define VK_DRESSI_H

#include "tr_local.h"

void R_Dressi_Init( void );
void R_Dressi_Shutdown( void );
qboolean vk_dressi_active( void );
void vk_dressi_record_pass( VkCommandBuffer cmd );

#endif /* VK_DRESSI_H */
