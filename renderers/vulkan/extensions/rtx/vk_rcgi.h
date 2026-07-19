#pragma once

#include "../common/vulkan/vulkan.h"

/*
 * Radiance Cache GI (RcGI) — spatial-hash radiance cache + cascaded probes.
 * Chocolate RTX path; real GPU work when USE_VULKAN_RTX + ray query + shared TLAS.
 */

void vk_rcgi_init( void );
void vk_rcgi_shutdown( void );
void vk_rcgi_frame_begin( void );
qboolean vk_rcgi_active( void );
void vk_rcgi_apply_after_geometry( void );
/* Hybrid1 fusion: irradiance ready for Hybrid1 composite (skip RcGI scene add). */
qboolean vk_rcgi_hybrid1_fusion_active( void );
VkImageView vk_rcgi_irradiance_view( void );
float vk_rcgi_fusion_strength( void );
