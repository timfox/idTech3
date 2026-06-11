/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Gaussian Ray Tracing (GRTX): Vulkan RT over 3D Gaussian proxy AABBs.
See docs/GAUSSIAN_RAY_TRACING_GRTX.md.
===========================================================================
*/

#ifndef VK_GRTX_H
#define VK_GRTX_H

#include "tr_local.h"

void R_GRTX_Init( void );
void R_GRTX_Shutdown( void );

void vk_grtx_init( void );
void vk_grtx_shutdown( void );
void vk_grtx_frame_begin( void );
void vk_grtx_on_map_load( const char *mapBaseName );
void vk_grtx_record_pass( VkCommandBuffer cmd );
qboolean vk_grtx_active( void );

#endif /* VK_GRTX_H */
