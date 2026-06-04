#pragma once

#include "../common/vulkan/vulkan.h"

#ifdef USE_VULKAN_RTX

void vk_pathtrace_init( void );
void vk_pathtrace_shutdown( void );
void vk_pathtrace_frame_begin( void );
qboolean vk_pathtrace_active( void );
void vk_pathtrace_record_pass( VkCommandBuffer cmd );

#else

void vk_pathtrace_init( void );
void vk_pathtrace_shutdown( void );
void vk_pathtrace_frame_begin( void );
qboolean vk_pathtrace_active( void );
void vk_pathtrace_record_pass( VkCommandBuffer cmd );

#endif
