/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include <array>
#include <cstring>

// vk is declared in vk.h as Vk_Instance vk (includes device, queue, etc.)

// Vulkan function pointers
extern PFN_vkCreateBuffer qvkCreateBuffer;
extern PFN_vkDestroyBuffer qvkDestroyBuffer;
extern PFN_vkGetBufferMemoryRequirements qvkGetBufferMemoryRequirements;
extern PFN_vkAllocateMemory qvkAllocateMemory;
extern PFN_vkFreeMemory qvkFreeMemory;
extern PFN_vkBindBufferMemory qvkBindBufferMemory;
extern PFN_vkCreateImageView qvkCreateImageView;
extern PFN_vkDestroyImageView qvkDestroyImageView;
extern PFN_vkCreateDescriptorSetLayout qvkCreateDescriptorSetLayout;
extern PFN_vkDestroyDescriptorSetLayout qvkDestroyDescriptorSetLayout;
extern PFN_vkCreatePipelineLayout qvkCreatePipelineLayout;
extern PFN_vkDestroyPipelineLayout qvkDestroyPipelineLayout;
extern PFN_vkCreateGraphicsPipelines qvkCreateGraphicsPipelines;
extern PFN_vkDestroyPipeline qvkDestroyPipeline;
extern PFN_vkCmdBindVertexBuffers qvkCmdBindVertexBuffers;
extern PFN_vkCmdBindIndexBuffer qvkCmdBindIndexBuffer;
extern PFN_vkCmdDrawIndexed qvkCmdDrawIndexed;
extern PFN_vkCmdBindPipeline qvkCmdBindPipeline;
extern PFN_vkCmdBindDescriptorSets qvkCmdBindDescriptorSets;
extern PFN_vkMapMemory qvkMapMemory;
extern PFN_vkUnmapMemory qvkUnmapMemory;
extern PFN_vkUpdateDescriptorSets qvkUpdateDescriptorSets;

// Forward declarations
extern "C" void vk_2d_flush(void);
extern shader_t *R_GetShaderByHandle(qhandle_t hShader);
extern PFN_vkUpdateDescriptorSets qvkUpdateDescriptorSets;

// 2D Rendering Pipeline for Vulkan Renderer
// Handles 2D graphics rendering with proper Vulkan pipelines

// 2D Vertex structure
struct vk_2d_vertex_t {
    float x, y;        // Position
    float u, v;        // Texture coordinates
    float r, g, b, a;  // Color
};

// 2D rendering pipeline state (removed - using vk_2d_internal only)

static struct vk_2d_pipeline_state {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    uint32_t maxVertices;
    uint32_t currentVertexCount;
    uint32_t currentIndexCount;

    std::array<vk_2d_vertex_t, 1024> vertexData;
    std::array<uint16_t, 1536> indexData; // 1024 * 1.5 for quads
    std::array<qhandle_t, 256> shaderHandles; // Track shader per quad (max 256 quads)

    qboolean initialized;
} vk_2d_internal = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                   VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                   1024, 0, 0, {}, {}, {}, qfalse};

// Initialize 2D rendering pipeline
qboolean vk_2d_initialize(void) {
    if (vk.device == (VkDevice)0x20000000) {
        ri.Printf(PRINT_ALL, "2D Vulkan: Skipping initialization (fake device)\n");
        vk_2d_internal.initialized = qtrue;
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "2D Vulkan: Initializing 2D rendering pipeline\n");

    // Create descriptor set layout for 2D rendering
    VkDescriptorSetLayoutBinding bindings[1] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = bindings
    };

    VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, nullptr, &vk_2d_internal.descriptorSetLayout));

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &vk_2d_internal.descriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    VK_CHECK(qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, nullptr, &vk_2d_internal.pipelineLayout));

    // Create graphics pipeline with 2D shaders
    // Note: This requires access to compiled SPIR-V shaders
    // For now, create a basic pipeline structure that will be completed later
    vk_2d_internal.pipeline = VK_NULL_HANDLE; // Will be implemented when shader loading is complete

    // Create vertex buffer
    VkBufferCreateInfo vertexBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = sizeof(vk_2d_vertex_t) * vk_2d_internal.vertexData.size(),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };

    VK_CHECK(qvkCreateBuffer(vk.device, &vertexBufferInfo, nullptr, &vk_2d_internal.vertexBuffer));

    // Allocate memory for vertex buffer
    VkMemoryRequirements memRequirements;
    qvkGetBufferMemoryRequirements(vk.device, vk_2d_internal.vertexBuffer, &memRequirements);

    // Find proper memory type for vertex buffer
    uint32_t memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };

    VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, nullptr, &vk_2d_internal.vertexBufferMemory));
    VK_CHECK(qvkBindBufferMemory(vk.device, vk_2d_internal.vertexBuffer, vk_2d_internal.vertexBufferMemory, 0));

    // Create index buffer
    VkBufferCreateInfo indexBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = sizeof(uint16_t) * vk_2d_internal.indexData.size(),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };

    VK_CHECK(qvkCreateBuffer(vk.device, &indexBufferInfo, nullptr, &vk_2d_internal.indexBuffer));

    qvkGetBufferMemoryRequirements(vk.device, vk_2d_internal.indexBuffer, &memRequirements);

    allocInfo.allocationSize = memRequirements.size;
    VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, nullptr, &vk_2d_internal.indexBufferMemory));
    VK_CHECK(qvkBindBufferMemory(vk.device, vk_2d_internal.indexBuffer, vk_2d_internal.indexBufferMemory, 0));

    vk_2d_internal.maxVertices = vk_2d_internal.vertexData.size();
    vk_2d_internal.currentVertexCount = 0;
    vk_2d_internal.currentIndexCount = 0;
    vk_2d_internal.initialized = qtrue;

    ri.Printf(PRINT_ALL, "2D Vulkan: 2D rendering pipeline initialized\n");
    return qtrue;
}

// Shutdown 2D rendering pipeline
void vk_2d_shutdown(void) {
    if (!vk_2d_internal.initialized || vk.device == (VkDevice)0x20000000) {
        return;
    }

    if (vk_2d_internal.indexBuffer) {
        qvkDestroyBuffer(vk.device, vk_2d_internal.indexBuffer, nullptr);
        vk_2d_internal.indexBuffer = VK_NULL_HANDLE;
    }

    if (vk_2d_internal.indexBufferMemory) {
        qvkFreeMemory(vk.device, vk_2d_internal.indexBufferMemory, nullptr);
        vk_2d_internal.indexBufferMemory = VK_NULL_HANDLE;
    }

    if (vk_2d_internal.vertexBuffer) {
        qvkDestroyBuffer(vk.device, vk_2d_internal.vertexBuffer, nullptr);
        vk_2d_internal.vertexBuffer = VK_NULL_HANDLE;
    }

    if (vk_2d_internal.vertexBufferMemory) {
        qvkFreeMemory(vk.device, vk_2d_internal.vertexBufferMemory, nullptr);
        vk_2d_internal.vertexBufferMemory = VK_NULL_HANDLE;
    }

    if (vk_2d_internal.pipeline) {
        qvkDestroyPipeline(vk.device, vk_2d_internal.pipeline, nullptr);
        vk_2d_internal.pipeline = VK_NULL_HANDLE;
    }

    if (vk_2d_internal.pipelineLayout) {
        qvkDestroyPipelineLayout(vk.device, vk_2d_internal.pipelineLayout, nullptr);
        vk_2d_internal.pipelineLayout = VK_NULL_HANDLE;
    }

    if (vk_2d_internal.descriptorSetLayout) {
        qvkDestroyDescriptorSetLayout(vk.device, vk_2d_internal.descriptorSetLayout, nullptr);
        vk_2d_internal.descriptorSetLayout = VK_NULL_HANDLE;
    }

    vk_2d_internal.initialized = qfalse;
    ri.Printf(PRINT_ALL, "2D Vulkan: 2D rendering pipeline shutdown\n");
}

// Add a textured quad to the 2D rendering batch
extern "C" void vk_2d_add_quad(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader) {
    if (!vk_2d_internal.initialized || vk.device == (VkDevice)0x20000000) {
        return;
    }

    // Check if we have space for 4 more vertices and 6 more indices
    if (vk_2d_internal.currentVertexCount + 4 > vk_2d_internal.maxVertices ||
        vk_2d_internal.currentIndexCount + 6 > vk_2d_internal.indexData.size()) {
        // Flush current batch
        vk_2d_flush();
    }

    uint16_t baseIndex = vk_2d_internal.currentVertexCount;
    uint32_t color = ((uint32_t)(vk.currentColor[3] * 255) << 24) |
                     ((uint32_t)(vk.currentColor[2] * 255) << 16) |
                     ((uint32_t)(vk.currentColor[1] * 255) << 8) |
                     ((uint32_t)(vk.currentColor[0] * 255));

    // Convert to float colors
    float r = vk.currentColor[0];
    float g = vk.currentColor[1];
    float b = vk.currentColor[2];
    float a = vk.currentColor[3];

    // Add vertices (clockwise winding)
    vk_2d_internal.vertexData[vk_2d_internal.currentVertexCount++] = {x, y, s1, t1, r, g, b, a};           // Top-left
    vk_2d_internal.vertexData[vk_2d_internal.currentVertexCount++] = {x + w, y, s2, t1, r, g, b, a};       // Top-right
    vk_2d_internal.vertexData[vk_2d_internal.currentVertexCount++] = {x + w, y + h, s2, t2, r, g, b, a};   // Bottom-right
    vk_2d_internal.vertexData[vk_2d_internal.currentVertexCount++] = {x, y + h, s1, t2, r, g, b, a};       // Bottom-left

    // Add indices for two triangles
    vk_2d_internal.indexData[vk_2d_internal.currentIndexCount++] = baseIndex;
    vk_2d_internal.indexData[vk_2d_internal.currentIndexCount++] = baseIndex + 1;
    vk_2d_internal.indexData[vk_2d_internal.currentIndexCount++] = baseIndex + 2;
    vk_2d_internal.indexData[vk_2d_internal.currentIndexCount++] = baseIndex;
    vk_2d_internal.indexData[vk_2d_internal.currentIndexCount++] = baseIndex + 2;
    vk_2d_internal.indexData[vk_2d_internal.currentIndexCount++] = baseIndex + 3;

    // Track shader handle for this quad (store at quad index = currentVertexCount / 4)
    uint32_t quadIndex = (vk_2d_internal.currentVertexCount - 4) / 4;
    if (quadIndex < vk_2d_internal.shaderHandles.size()) {
        vk_2d_internal.shaderHandles[quadIndex] = hShader;
    }
}

// Flush the current 2D rendering batch
extern "C" void vk_2d_flush(void) {
    if (!vk_2d_internal.initialized || vk.device == (VkDevice)0x20000000 ||
        vk_2d_internal.currentVertexCount == 0) {
        return;
    }

    // Update vertex buffer with current data
    void* data;
    VK_CHECK(qvkMapMemory(vk.device, vk_2d_internal.vertexBufferMemory, 0,
                         vk_2d_internal.currentVertexCount * sizeof(vk_2d_vertex_t), 0, &data));
    memcpy(data, vk_2d_internal.vertexData.data(),
           vk_2d_internal.currentVertexCount * sizeof(vk_2d_vertex_t));
    qvkUnmapMemory(vk.device, vk_2d_internal.vertexBufferMemory);

    // Update index buffer with current data
    VK_CHECK(qvkMapMemory(vk.device, vk_2d_internal.indexBufferMemory, 0,
                         vk_2d_internal.currentIndexCount * sizeof(uint16_t), 0, &data));
    memcpy(data, vk_2d_internal.indexData.data(),
           vk_2d_internal.currentIndexCount * sizeof(uint16_t));
    qvkUnmapMemory(vk.device, vk_2d_internal.indexBufferMemory);

    // Get current command buffer
    VkCommandBuffer cmdBuffer = vk.cmd->command_buffer;

    // Bind pipeline (when implemented)
    if (vk_2d_internal.pipeline != VK_NULL_HANDLE) {
        qvkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_2d_internal.pipeline);
    }

    // Bind vertex buffer
    VkDeviceSize offset = 0;
    qvkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vk_2d_internal.vertexBuffer, &offset);

    // Bind index buffer
    qvkCmdBindIndexBuffer(cmdBuffer, vk_2d_internal.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    // Update descriptor set with texture from first quad's shader
    if (vk_2d_internal.descriptorSet != VK_NULL_HANDLE && vk_2d_internal.currentVertexCount >= 4) {
        // Get shader from first quad
        qhandle_t firstShaderHandle = vk_2d_internal.shaderHandles[0];
        if (firstShaderHandle > 0) {
            shader_t *shader = R_GetShaderByHandle(firstShaderHandle);
            if (shader && shader->numUnfoggedPasses > 0 && shader->stages[0] && 
                shader->stages[0]->bundle[0].image[0]) {
                image_t *image = shader->stages[0]->bundle[0].image[0];
                if (image && image->handle != VK_NULL_HANDLE) {
                    // Update descriptor set with the image
                    VkDescriptorImageInfo imageInfo = {
                        .sampler = VK_NULL_HANDLE, // Use default sampler
                        .imageView = (VkImageView)image->handle, // Assuming handle is imageView
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    };
                    VkWriteDescriptorSet descriptorWrite = {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .pNext = nullptr,
                        .dstSet = vk_2d_internal.descriptorSet,
                        .dstBinding = 0,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .pImageInfo = &imageInfo,
                        .pBufferInfo = nullptr,
                        .pTexelBufferView = nullptr
                    };
                    qvkUpdateDescriptorSets(vk.device, 1, &descriptorWrite, 0, nullptr);
                }
            }
        }
    }

    // Bind descriptor sets
    if (vk_2d_internal.descriptorSet != VK_NULL_HANDLE) {
        qvkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk_2d_internal.pipelineLayout, 0, 1,
                                &vk_2d_internal.descriptorSet, 0, nullptr);
    }

    // Draw indexed
    qvkCmdDrawIndexed(cmdBuffer, vk_2d_internal.currentIndexCount, 1, 0, 0, 0);

    ri.Printf(PRINT_DEVELOPER, "2D Vulkan: Flushed %u vertices, %u indices\n",
              vk_2d_internal.currentVertexCount, vk_2d_internal.currentIndexCount);

    // Reset counters for next batch
    vk_2d_internal.currentVertexCount = 0;
    vk_2d_internal.currentIndexCount = 0;
}