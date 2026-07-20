/*
===========================================================================
Cinematic Engine Platform 1.0 — LTC area-light LUTs (Environment Slice).
Uploads canonical mat/amp tables from ltc_tables.h for Forward+ / deferred.
===========================================================================
*/

#pragma once

#ifdef USE_VULKAN

#include "vk.h"

typedef struct vkLtcState_s {
	qboolean uploaded;
	VkImage matImage;
	VkImageView matView;
	VkDeviceMemory matMemory;
	VkImage ampImage;
	VkImageView ampView;
	VkDeviceMemory ampMemory;
	VkSampler sampler;
} vkLtcState_t;

void vk_ltc_init( void );
void vk_ltc_shutdown( void );
qboolean vk_ltc_uploaded( void );
const vkLtcState_t *vk_ltc_state( void );

/* Bind LTC mat/amp into Forward+ graphics set (bindings 6/7) and deferred lighting (8/9). */
void vk_ltc_update_forward_plus_descriptors( VkDescriptorSet set );
void vk_ltc_update_deferred_lighting_descriptors( VkDescriptorSet set );

void vk_ltc_status_f( void );

#endif /* USE_VULKAN */
