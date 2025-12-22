#ifndef __VK_FRAMEBUFFER_H__
#define __VK_FRAMEBUFFER_H__

#include <vulkan/vulkan.h>
#include "vk.h"

// Framebuffer and render pass management function declarations
void vk_create_render_passes(void);
void vk_create_framebuffers(void);
void vk_destroy_framebuffers(void);
void vk_create_prefilter_framebuffer(filterDef *def);

// Render pass transition utilities
void vk_begin_main_render_pass(void);
void vk_end_main_render_pass(void);
void vk_begin_post_bloom_render_pass(void);
void vk_end_post_bloom_render_pass(void);

// Framebuffer state queries
qboolean vk_has_framebuffers(void);
uint32_t vk_get_framebuffer_count(void);

#endif // __VK_FRAMEBUFFER_H__
