#ifndef __VK_DRAW_H__
#define __VK_DRAW_H__

#include "vk.h"

#ifdef __cplusplus
extern "C" {
#endif

// Drawing and rendering functions
#ifdef USE_VBO
void vk_draw_indexed(uint32_t indexCount, uint32_t firstIndex);
#endif

void vk_draw_geometry(Vk_Depth_Range depth_range, qboolean indexed);
void vk_draw_dot(uint32_t storage_offset);

// Descriptor and pipeline binding
void vk_bind_specific_descriptor_sets(VkPipelineBindPoint bind_point, VkPipelineLayout layout,
    uint32_t first_set, uint32_t descriptor_set_count, const VkDescriptorSet* descriptor_sets,
    uint32_t dynamic_offset_count, const uint32_t* dynamic_offsets);

// Viewport and scissor management
void vk_set_viewport(float x, float y, float width, float height, float min_depth, float max_depth);
void vk_set_scissor(int x, int y, uint32_t width, uint32_t height);
void vk_set_depth_range(Vk_Depth_Range depth_range);

// Render pass management is in vk_renderpass.h

// Blend state
void vk_set_blend_constants(float r, float g, float b, float a);

#ifdef __cplusplus
}
#endif

#endif // __VK_DRAW_H__
