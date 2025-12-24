#ifndef __VK_FRAME_H__
#define __VK_FRAME_H__

#include "vk.h"

// Frame management
void vk_begin_frame(void);
void vk_end_frame(void);
void vk_present_frame(void);

// Resource management
void vk_resize_frame_resources(uint32_t width, uint32_t height);

// Clear operations
void vk_clear_color(const vec4_t clear_color);
void vk_clear_depth(qboolean clear_stencil);

// Pixel reading
void vk_read_pixels(byte *buffer, uint32_t width, uint32_t height);

#endif // __VK_FRAME_H__
