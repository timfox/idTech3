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
#include <array>

// External Vulkan objects (declared in initialization module)
extern VkInstance vk_instance;
extern VkPhysicalDevice vk_physical_device;
extern VkDevice vk_device;
extern VkQueue vk_queue;
extern uint32_t vk_queue_family_index;

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

// 2D Rendering Pipeline for Vulkan Renderer
// Handles 2D graphics rendering with proper Vulkan pipelines

// 2D Vertex structure
struct vk_2d_vertex_t {
    float x, y;        // Position
    float u, v;        // Texture coordinates
    float r, g, b, a;  // Color
};

// 2D rendering pipeline state
vk_2d_state_t vk_2d = {qfalse};

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

    qboolean initialized;
} vk_2d_internal = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                   VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                   1024, 0, 0, {}, {}, qfalse};

// Initialize 2D rendering pipeline
qboolean vk_2d_initialize(void) {
    if (vk_device == (VkDevice)0x20000000) {
        ri.Printf(PRINT_ALL, "2D Vulkan: Skipping initialization (fake device)\n");
        vk_2d.initialized = qtrue; // Mark as initialized to avoid repeated attempts
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

    VK_CHECK(qvkCreateDescriptorSetLayout(vk_device, &layoutInfo, nullptr, &vk_2d_internal.descriptorSetLayout));

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

    VK_CHECK(qvkCreatePipelineLayout(vk_device, &pipelineLayoutInfo, nullptr, &vk_2d_internal.pipelineLayout));

    // TODO: Create actual graphics pipeline with vertex/fragment shaders
    // For now, we'll create a basic pipeline structure
    vk_2d_internal.pipeline = VK_NULL_HANDLE; // Placeholder

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

    VK_CHECK(qvkCreateBuffer(vk_device, &vertexBufferInfo, nullptr, &vk_2d_internal.vertexBuffer));

    // Allocate memory for vertex buffer
    VkMemoryRequirements memRequirements;
    qvkGetBufferMemoryRequirements(vk_device, vk_2d_internal.vertexBuffer, &memRequirements);

    // TODO: Proper memory allocation using VMA
    // For now, use a simple allocation
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = 0 // TODO: Find proper memory type
    };

    VK_CHECK(qvkAllocateMemory(vk_device, &allocInfo, nullptr, &vk_2d_internal.vertexBufferMemory));
    VK_CHECK(qvkBindBufferMemory(vk_device, vk_2d_internal.vertexBuffer, vk_2d_internal.vertexBufferMemory, 0));

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

    VK_CHECK(qvkCreateBuffer(vk_device, &indexBufferInfo, nullptr, &vk_2d_internal.indexBuffer));

    qvkGetBufferMemoryRequirements(vk_device, vk_2d_internal.indexBuffer, &memRequirements);

    allocInfo.allocationSize = memRequirements.size;
    VK_CHECK(qvkAllocateMemory(vk_device, &allocInfo, nullptr, &vk_2d_internal.indexBufferMemory));
    VK_CHECK(qvkBindBufferMemory(vk_device, vk_2d_internal.indexBuffer, vk_2d_internal.indexBufferMemory, 0));

    vk_2d_internal.maxVertices = vk_2d_internal.vertexData.size();
    vk_2d_internal.currentVertexCount = 0;
    vk_2d_internal.currentIndexCount = 0;
    vk_2d_internal.initialized = qtrue;

    ri.Printf(PRINT_ALL, "2D Vulkan: 2D rendering pipeline initialized\n");
    return qtrue;
}

// Shutdown 2D rendering pipeline
void vk_2d_shutdown(void) {
    if (!vk_2d_internal.initialized || vk_device == (VkDevice)0x20000000) {
        return;
    }

    if (vk_2d_internal.indexBuffer) {
        qvkDestroyBuffer(vk_device, vk_2d_internal.indexBuffer, nullptr);
        vk_2d_internal.indexBuffer = VK_NULL_HANDLE;
    }

    if (vk_2d_internal.indexBufferMemory) {
        qvkFreeMemory(vk_device, vk_2d_internal.indexBufferMemory, nullptr);
        vk_2d_internal.indexBufferMemory = VK_NULL_HANDLE;
    }

    if (vk_2d_internal.vertexBuffer) {
        qvkDestroyBuffer(vk_device, vk_2d_internal.vertexBuffer, nullptr);
        vk_2d_internal.vertexBuffer = VK_NULL_HANDLE;
    }

    if (vk_2d_internal.vertexBufferMemory) {
        qvkFreeMemory(vk_device, vk_2d_internal.vertexBufferMemory, nullptr);
        vk_2d_internal.vertexBufferMemory = VK_NULL_HANDLE;
    }

    if (vk_2d_internal.pipeline) {
        qvkDestroyPipeline(vk_device, vk_2d_internal.pipeline, nullptr);
        vk_2d_internal.pipeline = VK_NULL_HANDLE;
    }

    if (vk_2d_internal.pipelineLayout) {
        qvkDestroyPipelineLayout(vk_device, vk_2d_internal.pipelineLayout, nullptr);
        vk_2d_internal.pipelineLayout = VK_NULL_HANDLE;
    }

    if (vk_2d_internal.descriptorSetLayout) {
        qvkDestroyDescriptorSetLayout(vk_device, vk_2d_internal.descriptorSetLayout, nullptr);
        vk_2d_internal.descriptorSetLayout = VK_NULL_HANDLE;
    }

    vk_2d_internal.initialized = qfalse;
    ri.Printf(PRINT_ALL, "2D Vulkan: 2D rendering pipeline shutdown\n");
}

// Add a textured quad to the 2D rendering batch
void vk_2d_add_quad(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader) {
    if (!vk_2d_internal.initialized || vk_device == (VkDevice)0x20000000) {
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

    Q_UNUSED(hShader); // TODO: Use shader for texture binding
}

// Flush the current 2D rendering batch
void vk_2d_flush(void) {
    if (!vk_2d.initialized || vk_device == (VkDevice)0x20000000 ||
        vk_2d_internal.currentVertexCount == 0) {
        return;
    }

    // TODO: Implement actual Vulkan rendering commands
    // 1. Update vertex/index buffers with current data
    // 2. Bind pipeline and descriptor sets
    // 3. Record draw commands
    // 4. Submit to command buffer

    ri.Printf(PRINT_DEVELOPER, "2D Vulkan: Flushing %u vertices, %u indices\n",
              vk_2d_internal.currentVertexCount, vk_2d_internal.currentIndexCount);

    // Reset counters for next batch
    vk_2d_internal.currentVertexCount = 0;
    vk_2d_internal.currentIndexCount = 0;
}