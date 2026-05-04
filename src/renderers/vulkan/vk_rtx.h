#pragma once

#include "../common/vulkan/vulkan.h"

#ifdef USE_VULKAN_RTX

void vk_rtx_init( void );
void vk_rtx_shutdown( void );
void vk_rtx_frame_begin( void );
void vk_rtx_record_demo_pass( VkCommandBuffer cmd );

#else

void vk_rtx_init( void );
void vk_rtx_shutdown( void );
void vk_rtx_frame_begin( void );
void vk_rtx_record_demo_pass( VkCommandBuffer cmd );

#endif
