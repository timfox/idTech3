#pragma once

#include "../common/vulkan/vulkan.h"

#ifdef USE_VULKAN_RTX

void vk_hybrid1_init( void );
void vk_hybrid1_shutdown( void );
void vk_hybrid1_frame_begin( void );
qboolean vk_hybrid1_active( void );
void vk_hybrid1_record_pass( VkCommandBuffer cmd );

#else

void vk_hybrid1_init( void );
void vk_hybrid1_shutdown( void );
void vk_hybrid1_frame_begin( void );
qboolean vk_hybrid1_active( void );
void vk_hybrid1_record_pass( VkCommandBuffer cmd );

#endif
