#include "vk_draw.h"
#include "vk_utils.h"
#include "vk_descriptors.h"
#include "vk_pipeline.h"
#include "../renderercommon/tr_public.h"
#include "vk.h"
#include "../../common/performance_counters.h"

// Renderer interface
extern refimport_t ri;

// Vulkan function pointer extern declarations
extern PFN_vkCmdSetBlendConstants qvkCmdSetBlendConstants;
extern PFN_vkCmdDraw qvkCmdDraw;
extern PFN_vkCmdDrawIndexed qvkCmdDrawIndexed;
extern PFN_vkCmdBindVertexBuffers qvkCmdBindVertexBuffers;
extern PFN_vkCmdBindIndexBuffer qvkCmdBindIndexBuffer;
extern PFN_vkCmdBindDescriptorSets qvkCmdBindDescriptorSets;
extern PFN_vkCmdSetViewport qvkCmdSetViewport;
extern PFN_vkCmdSetScissor qvkCmdSetScissor;

// External references
extern shaderCommands_t tess;
extern backEndState_t backEnd;

// Performance counter wrapper for draw calls
static inline void vk_draw_call(VkCommandBuffer cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    Perf_CountDrawCall();
    qvkCmdDraw(cmd, vertexCount, instanceCount, firstVertex, firstInstance);
}

static inline void vk_draw_indexed_call(VkCommandBuffer cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    Perf_CountDrawCall();
    qvkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

#ifdef USE_VBO
// Draw indexed geometry using VBO
void vk_draw_indexed(uint32_t indexCount, uint32_t firstIndex) {
    vk_draw_indexed_call(vk.cmd->command_buffer, indexCount, 1, firstIndex, 0, 0);
}
#endif

// Draw geometry (indexed or non-indexed)
void vk_draw_geometry(Vk_Depth_Range depth_range, qboolean indexed) {
    if (!vk_validate_handle(vk.cmd->command_buffer, "command buffer")) {
        return;
    }

#ifdef USE_VBO
    if (vk.geometry_buffer_size_new) {
        VkBuffer vertex_buffers[1] = {vk.vbo.vertex_buffer};
        VkDeviceSize vertex_offset[1] = {vk.cmd->vertex_buffer_offset};

        qvkCmdBindVertexBuffers(vk.cmd->command_buffer, 0, 1, vertex_buffers, vertex_offset);

        if (indexed) {
            qvkCmdBindIndexBuffer(vk.cmd->command_buffer, vk.cmd->curr_index_buffer, vk.cmd->curr_index_offset, VK_INDEX_TYPE_UINT32);
        }
    } else
#endif
    {
        VkBuffer vertex_buffers[1] = {vk.vbo.vertex_buffer};
        VkDeviceSize vertex_offset[1] = {0};

        qvkCmdBindVertexBuffers(vk.cmd->command_buffer, 0, 1, vertex_buffers, vertex_offset);

        if (indexed) {
            qvkCmdBindIndexBuffer(vk.cmd->command_buffer, vk.cmd->curr_index_buffer, 0, VK_INDEX_TYPE_UINT32);
        }
    }

    if (vk.geometry_buffer_size_new) {
        vk_update_uniform_descriptor(vk.cmd->uniform_descriptor, vk.vbo.vertex_buffer);
    }

    vk_set_depth_range(depth_range);

    if (indexed) {
        vk_draw_indexed_call(vk.cmd->command_buffer, vk.cmd->num_indexes, 1, 0, 0, 0);
    } else {
        vk_draw_call(vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0);
    }
}

// Draw dot (for debugging)
void vk_draw_dot(uint32_t /*storage_offset*/) {
    if (!vk_validate_handle(vk.cmd->command_buffer, "command buffer")) {
        return;
    }

    if (vk.geometry_buffer_size_new) {
        VkBuffer vertex_buffers[1] = {vk.vbo.vertex_buffer};
        VkDeviceSize vertex_offset[1] = {vk.cmd->vertex_buffer_offset};

        qvkCmdBindVertexBuffers(vk.cmd->command_buffer, 0, 1, vertex_buffers, vertex_offset);
    }

    vk_set_depth_range(DEPTH_RANGE_NORMAL);

    vk_draw_call(vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0);
}

// Bind descriptor sets for rendering
void vk_bind_specific_descriptor_sets(VkPipelineBindPoint bind_point, VkPipelineLayout layout,
    uint32_t first_set, uint32_t descriptor_set_count, const VkDescriptorSet* descriptor_sets,
    uint32_t dynamic_offset_count, const uint32_t* dynamic_offsets) {

    if (!vk_validate_handle(layout, "pipeline layout")) {
        return;
    }

    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, bind_point, layout,
        first_set, descriptor_set_count, descriptor_sets,
        dynamic_offset_count, dynamic_offsets);
}

// Update viewport
void vk_set_viewport(float x, float y, float width, float height, float min_depth, float max_depth) {
    VkViewport viewport = {
        .x = x,
        .y = y,
        .width = vk_sanitize_float(width, 1.0f),
        .height = vk_sanitize_float(height, 1.0f),
        .minDepth = vk_sanitize_float(min_depth, 0.0f),
        .maxDepth = vk_sanitize_float(max_depth, 1.0f)
    };

    qvkCmdSetViewport(vk.cmd->command_buffer, 0, 1, &viewport);
}

// Update scissor
void vk_set_scissor(int x, int y, uint32_t width, uint32_t height) {
    VkRect2D scissor = {
        .offset = {x, y},
        .extent = {width, height}
    };

    qvkCmdSetScissor(vk.cmd->command_buffer, 0, 1, &scissor);
}

// Update depth range
void vk_set_depth_range(Vk_Depth_Range depth_range) {
    float n, f;

    switch (depth_range) {
        case DEPTH_RANGE_NORMAL:
            n = 0.0f;
            f = 1.0f;
            break;
        case DEPTH_RANGE_ZERO:
            n = 0.0f;
            f = 0.0f;
            break;
        case DEPTH_RANGE_ONE:
            n = 1.0f;
            f = 1.0f;
            break;
        case DEPTH_RANGE_WEAPON:
            n = 0.0f;
            f = 0.3f;
            break;
        default:
            ri.Printf(PRINT_WARNING, "vk_update_depth_range: unknown depth range %d\n", depth_range);
            n = 0.0f;
            f = 1.0f;
            break;
    }

    vk_set_viewport(0, 0, vk.renderWidth, vk.renderHeight, n, f);
}

// Begin rendering to a render pass
void vk_begin_render_pass(VkRenderPass render_pass, VkFramebuffer framebuffer,
    uint32_t width, uint32_t height, uint32_t clear_value_count, const VkClearValue* clear_values) {

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = render_pass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = {width, height}
        },
        .clearValueCount = clear_value_count,
        .pClearValues = clear_values
    };

    qvkCmdBeginRenderPass(vk.cmd->command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}


// Set blend constants
void vk_set_blend_constants(float r, float g, float b, float a) {
    float blend_constants[4] = {
        vk_sanitize_float(r, 0.0f),
        vk_sanitize_float(g, 0.0f),
        vk_sanitize_float(b, 0.0f),
        vk_sanitize_float(a, 1.0f)
    };

    qvkCmdSetBlendConstants(vk.cmd->command_buffer, blend_constants);
}
