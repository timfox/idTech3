/*
=============================================================================
Procedural Dressing System

Generates instancing directives from paint/spline/volume rules and pushes
precomputed transforms into the GPU instancing path.
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include "vk.h"
#include "vk_gpu_culling.h"

#ifdef USE_VULKAN

#define VK_MAX_PROC_RULES     64
#define VK_MAX_PROC_BIOMES    8
#define VK_MAX_PROC_INSTANCES 65536

void vk_proc_dressing_init( void );
void vk_proc_dressing_shutdown( void );
void vk_proc_dressing_tick( void ); // called each frame before gpu culling upload
void vk_proc_dressing_mark_dirty( void );

#endif // USE_VULKAN


