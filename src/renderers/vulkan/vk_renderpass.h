#ifndef __VK_RENDERPASS_H__
#define __VK_RENDERPASS_H__

#include "vk.h"

// Render pass creation and management
qboolean vk_create_main_render_pass(void);
qboolean vk_create_screenmap_render_pass(void);

// Framebuffer management
VkFramebuffer vk_create_framebuffer(VkRenderPass render_pass, uint32_t attachment_count,
    const VkImageView* attachments, uint32_t width, uint32_t height);
void vk_destroy_framebuffer(VkFramebuffer framebuffer);
void vk_destroy_render_pass(VkRenderPass render_pass);

// Render pass operations
void vk_begin_specific_render_pass(VkRenderPass render_pass, VkFramebuffer framebuffer,
    qboolean clear_values, uint32_t width, uint32_t height);
void vk_end_render_pass(void);
void vk_next_subpass(void);

// Specialized render passes
void vk_begin_bloom_extract_render_pass(void);
void vk_begin_blur_render_pass(uint32_t index);

// Memory barriers
void vk_barrier_final_image_to_shader_read(VkImage image);

#endif // __VK_RENDERPASS_H__
