#ifndef VK_RENDER_PASS_H
#define VK_RENDER_PASS_H

#include "../common/vulkan/vulkan.h"

void vk_set_fullscreen_viewport_scissor( uint32_t width, uint32_t height );
void vk_begin_render_pass_tracked( VkRenderPass renderPass, VkFramebuffer frameBuffer, qboolean clearValues, uint32_t width, uint32_t height );
void vk_end_render_pass_tracked( void );
void vk_create_render_passes( void );

#endif
