#pragma once

#include "../common/vulkan/vulkan.h"

/*
 * Surfel GI (GIBS) — Global Illumination Based on Surfels.
 * Chocolate RTX path; real GPU work when USE_VULKAN_RTX + ray query + shared TLAS.
 */

void vk_surfel_gi_init( void );
void vk_surfel_gi_shutdown( void );
void vk_surfel_gi_frame_begin( void );
qboolean vk_surfel_gi_active( void );
void vk_surfel_gi_apply_after_geometry( void );
