#include "vk_compute_raytracing.h"
#include "vk.h"
#include "vk_rtx_acceleration.h"
#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "../../common/qcommon.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

vk_compute_rt_t vk_compute_rt;

// Scene objects storage (temporary, will be uploaded to GPU)
#define MAX_SCENE_OBJECTS 256
static rt_scene_object_t sceneObjects[MAX_SCENE_OBJECTS];
static int sceneObjectCount = 0;
static uint32_t currentObjectId = 0;

void VK_ComputeRT_Init(void) {
    // If already initialized, nothing to do
    if (vk_compute_rt.initialized) {
        ri.Printf(PRINT_DEVELOPER, "VK_ComputeRT_Init: already initialized\n");
        return;
    }
    // Reset state for safety
    memset(&vk_compute_rt, 0, sizeof(vk_compute_rt_t));

    // Create compute command pool
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = vk.queue_family_index;  // Use graphics queue for compute
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(qvkCreateCommandPool(vk.device, &poolInfo, NULL, &vk_compute_rt.computeCommandPool));

    // Allocate compute command buffer
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vk_compute_rt.computeCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VK_CHECK(qvkAllocateCommandBuffers(vk.device, &allocInfo, &vk_compute_rt.computeCommandBuffer));

    // Create fence
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(vk.device, &fenceInfo, NULL, &vk_compute_rt.computeFence));

    // Create descriptor set layout for compute shader
    VkDescriptorSetLayoutBinding bindings[3] = {};

    // Storage image (output)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Uniform buffer
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Scene objects storage buffer
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = bindings;

    VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk_compute_rt.computeDescriptorSetLayout));
    // Create a timestamp query pool for per-dispatch timing (2 timestamps)
    VkQueryPoolCreateInfo qpInfo = {};
    qpInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qpInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qpInfo.queryCount = 2;
    VK_CHECK(vkCreateQueryPool(vk.device, &qpInfo, NULL, &vk_compute_rt.computeQueryPool));
    // Retrieve timestamp period for conversion to nanoseconds
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(vk.physicalDevice, &props);
    vk_compute_rt.timestampPeriod = props.limits.timestampPeriod;

    // Descriptor pool and set for compute bindings
    VkDescriptorPoolSize poolSizes[3] = { {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
                                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} };
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;
    VK_CHECK(vkCreateDescriptorPool(vk.device, &poolInfo, NULL, &vk_compute_rt.computeDescriptorPool));

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = vk_compute_rt.computeDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &vk_compute_rt.computeDescriptorSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(vk.device, &allocInfo, &vk_compute_rt.computeDescriptorSet));

    // Initial descriptor writes for bindings
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageView = vk_compute_rt.storageImageView;
    imageInfo.sampler = vk_compute_rt.storageImageSampler;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorBufferInfo uniformBufInfo = {};
    uniformBufInfo.buffer = vk_compute_rt.uniformBuffer;
    uniformBufInfo.offset = 0;
    uniformBufInfo.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo sceneBufInfo = {};
    sceneBufInfo.buffer = vk_compute_rt.sceneObjectsBuffer;
    sceneBufInfo.offset = 0;
    sceneBufInfo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet writes[3] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = vk_compute_rt.computeDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[0].pImageInfo = &imageInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = vk_compute_rt.computeDescriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].pBufferInfo = &uniformBufInfo;
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = vk_compute_rt.computeDescriptorSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].pBufferInfo = &sceneBufInfo;
    vkUpdateDescriptorSets(vk.device, 3, writes, 0, NULL);

    // Descriptor pool and set for compute bindings
    VkDescriptorPoolSize poolSizes[3] = { {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
                                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} };
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;
    VK_CHECK(vkCreateDescriptorPool(vk.device, &poolInfo, NULL, &vk_compute_rt.computeDescriptorPool));

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = vk_compute_rt.computeDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &vk_compute_rt.computeDescriptorSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(vk.device, &allocInfo, &vk_compute_rt.computeDescriptorSet));

    // Initial descriptor writes for bindings
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageView = vk_compute_rt.storageImageView;
    imageInfo.sampler = vk_compute_rt.storageImageSampler;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorBufferInfo uniformBufInfo = { vk_compute_rt.uniformBuffer, 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo sceneBufInfo = { vk_compute_rt.sceneObjectsBuffer, 0, VK_WHOLE_SIZE };
    VkWriteDescriptorSet writes[3] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = vk_compute_rt.computeDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[0].pImageInfo = &imageInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = vk_compute_rt.computeDescriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].pBufferInfo = &uniformBufInfo;
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = vk_compute_rt.computeDescriptorSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].pBufferInfo = &sceneBufInfo;
    vkUpdateDescriptorSets(vk.device, 3, writes, 0, NULL);

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk_compute_rt.computeDescriptorSetLayout;

    VK_CHECK(qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk_compute_rt.computePipelineLayout));

    // Create uniform buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(vec3_t) * 2 + sizeof(float) * 3 + sizeof(vec4_t);  // lightPos, camera pos/lookat, fov, aspect, fogColor
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(qvkCreateBuffer(vk.device, &bufferInfo, NULL, &vk_compute_rt.uniformBuffer));

    VkMemoryRequirements memReq;
    qvkGetBufferMemoryRequirements(vk.device, vk_compute_rt.uniformBuffer, &memReq);

    VkMemoryAllocateInfo allocMemInfo = {};
    allocMemInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocMemInfo.allocationSize = memReq.size;
    allocMemInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocMemInfo, NULL, &vk_compute_rt.uniformBufferMemory));
    VK_CHECK(qvkBindBufferMemory(vk.device, vk_compute_rt.uniformBuffer, vk_compute_rt.uniformBufferMemory, 0));

    VK_CHECK(qvkMapMemory(vk.device, vk_compute_rt.uniformBufferMemory, 0, bufferInfo.size, 0, &vk_compute_rt.uniformBufferMapped));

    // Create scene objects buffer
    VkBufferCreateInfo sceneBufferInfo = {};
    sceneBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    sceneBufferInfo.size = MAX_SCENE_OBJECTS * sizeof(rt_scene_object_t);
    sceneBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    sceneBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(qvkCreateBuffer(vk.device, &sceneBufferInfo, NULL, &vk_compute_rt.sceneObjectsBuffer));

    qvkGetBufferMemoryRequirements(vk.device, vk_compute_rt.sceneObjectsBuffer, &memReq);

    allocMemInfo.allocationSize = memReq.size;
    allocMemInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocMemInfo, NULL, &vk_compute_rt.sceneObjectsBufferMemory));
    VK_CHECK(qvkBindBufferMemory(vk.device, vk_compute_rt.sceneObjectsBuffer, vk_compute_rt.sceneObjectsBufferMemory, 0));

    // Default settings
    vk_compute_rt.resolution = 2048;
    vk_compute_rt.useReflections = qtrue;
    vk_compute_rt.maxBounces = 3;
    VectorSet(vk_compute_rt.cameraPos, 0.0f, 0.0f, -4.0f);
    VectorSet(vk_compute_rt.cameraLookat, 0.0f, 0.5f, 0.0f);
    vk_compute_rt.cameraFOV = 10.0f;
    VectorSet(vk_compute_rt.lightPos, 0.0f, 0.0f, 0.0f);
    vk_compute_rt.fogColor[0] = 0.0f;
    vk_compute_rt.fogColor[1] = 0.0f;
    vk_compute_rt.fogColor[2] = 0.0f;
    vk_compute_rt.fogColor[3] = 0.0f;

    vk_compute_rt.enabled = qtrue;
    vk_compute_rt.initialized = qtrue;

    // Attempt to load a SPIR-V compute shader (optional)
    const char* shaderPath = "./shaders/compute_raytracing.comp.spv";
    FILE* shaderFile = fopen(shaderPath, "rb");
    if (shaderFile) {
        fseek(shaderFile, 0, SEEK_END);
        long shaderSize = ftell(shaderFile);
        rewind(shaderFile);
        if (shaderSize > 0) {
            uint32_t* shaderCode = (uint32_t*)malloc(shaderSize);
            if (shaderCode && fread(shaderCode, 1, shaderSize, shaderFile) == (size_t)shaderSize) {
                VkShaderModuleCreateInfo smci = {};
                smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                smci.codeSize = (size_t)shaderSize;
                smci.pCode = shaderCode;
                if (vkCreateShaderModule(vk.device, &smci, NULL, &vk_compute_rt.computeShaderModule) == VK_SUCCESS) {
                    vk_compute_rt.computeShaderLoaded = qtrue;
                    ri.Printf(PRINT_ALL, "VK_ComputeRT_Init: loaded shader module (%s)\n", shaderPath);
                    // Create compute pipeline
                    VkPipelineShaderStageCreateInfo stage = {};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                    stage.module = vk_compute_rt.computeShaderModule;
                    stage.pName = "main";
                    VkComputePipelineCreateInfo cpi = {};
                    cpi.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                    cpi.stage = stage;
                    cpi.layout = vk_compute_rt.computePipelineLayout;
                    if (vkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &cpi, NULL, &vk_compute_rt.computePipeline) == VK_SUCCESS) {
                        ri.Printf(PRINT_ALL, "VK_ComputeRT_Init: compute pipeline created\n");
                    } else {
                        ri.Printf(PRINT_WARNING, "VK_ComputeRT_Init: failed to create compute pipeline\n");
                    }
                } else {
                    ri.Printf(PRINT_WARNING, "VK_ComputeRT_Init: failed to create shader module\n");
                }
            } else {
                ri.Printf(PRINT_WARNING, "VK_ComputeRT_Init: failed to read shader file\n");
            }
            free(shaderCode);
        } else {
            ri.Printf(PRINT_WARNING, "VK_ComputeRT_Init: shader file empty\n");
        }
        fclose(shaderFile);
    } else {
        ri.Printf(PRINT_WARNING, "VK_ComputeRT_Init: shader not found: %s\n", shaderPath);
    }

    // Initialize optional hardware acceleration path (stubbed if not available)
    if (!vk_rtx_acceleration_init()) {
        ri.Printf(PRINT_WARNING, "Vulkan RTX: acceleration init failed or not available (stub).\n");
    } else {
        ri.Printf(PRINT_ALL, "Vulkan RTX: acceleration subsystem initialized (stub).\n");
    }
    ri.Printf(PRINT_ALL, "Compute ray tracing system initialized\n");
    // Route compute work through the main queue for now
    vk_compute_rt.computeQueue = vk.queue;
}

void VK_ComputeRT_Shutdown(void) {
    if (!vk_compute_rt.initialized) return;
    ri.Printf(PRINT_ALL, "VK_ComputeRT_Shutdown: tearing down compute RT path\n");
    // Destroy shader module if loaded
    if (vk_compute_rt.computeShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(vk.device, vk_compute_rt.computeShaderModule, NULL);
        vk_compute_rt.computeShaderModule = VK_NULL_HANDLE;
        vk_compute_rt.computeShaderLoaded = qfalse;
    }
    if (!vk_compute_rt.initialized) return;

    // Unmap uniform buffer
    if (vk_compute_rt.uniformBufferMapped) {
        qvkUnmapMemory(vk.device, vk_compute_rt.uniformBufferMemory);
    }

    // Destroy uniform buffer
    if (vk_compute_rt.uniformBuffer != VK_NULL_HANDLE) {
        qvkDestroyBuffer(vk.device, vk_compute_rt.uniformBuffer, NULL);
    }
    if (vk_compute_rt.uniformBufferMemory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk_compute_rt.uniformBufferMemory, NULL);
    }

    // Destroy scene objects buffer
    if (vk_compute_rt.sceneObjectsBuffer != VK_NULL_HANDLE) {
        qvkDestroyBuffer(vk.device, vk_compute_rt.sceneObjectsBuffer, NULL);
    }
    if (vk_compute_rt.sceneObjectsBufferMemory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk_compute_rt.sceneObjectsBufferMemory, NULL);
    }

    // Destroy storage image
    if (vk_compute_rt.storageImageSampler != VK_NULL_HANDLE) {
        qvkDestroySampler(vk.device, vk_compute_rt.storageImageSampler, NULL);
    }
    if (vk_compute_rt.storageImageView != VK_NULL_HANDLE) {
        qvkDestroyImageView(vk.device, vk_compute_rt.storageImageView, NULL);
    }
    if (vk_compute_rt.storageImage != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk_compute_rt.storageImage, NULL);
    }
    if (vk_compute_rt.storageImageMemory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk_compute_rt.storageImageMemory, NULL);
    }

    // Destroy compute pipeline
    if (vk_compute_rt.computePipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk_compute_rt.computePipeline, NULL);
    }
    if (vk_compute_rt.computePipelineLayout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk_compute_rt.computePipelineLayout, NULL);
    }
    if (vk_compute_rt.computeDescriptorSetLayout != VK_NULL_HANDLE) {
        qvkDestroyDescriptorSetLayout(vk.device, vk_compute_rt.computeDescriptorSetLayout, NULL);
    }

    // Destroy graphics pipeline
    if (vk_compute_rt.graphicsPipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk_compute_rt.graphicsPipeline, NULL);
    }
    if (vk_compute_rt.graphicsPipelineLayout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk_compute_rt.graphicsPipelineLayout, NULL);
    }
    if (vk_compute_rt.graphicsDescriptorSetLayout != VK_NULL_HANDLE) {
        qvkDestroyDescriptorSetLayout(vk.device, vk_compute_rt.graphicsDescriptorSetLayout, NULL);
    }

    // Destroy fence
    if (vk_compute_rt.computeFence != VK_NULL_HANDLE) {
        vkDestroyFence(vk.device, vk_compute_rt.computeFence, NULL);
    }

    // Destroy command pool
    if (vk_compute_rt.computeCommandPool != VK_NULL_HANDLE) {
        qvkDestroyCommandPool(vk.device, vk_compute_rt.computeCommandPool, NULL);
    }
    // Destroy timestamp query pool
    if (vk_compute_rt.computeQueryPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(vk.device, vk_compute_rt.computeQueryPool, NULL);
        vk_compute_rt.computeQueryPool = VK_NULL_HANDLE;
    }

    memset(&vk_compute_rt, 0, sizeof(vk_compute_rt_t));
    sceneObjectCount = 0;
    currentObjectId = 0;
}

void VK_ComputeRT_CreateStorageImage(int width, int height) {
    if (!vk_compute_rt.initialized) return;

    vk_compute_rt.storageImageWidth = width;
    vk_compute_rt.storageImageHeight = height;

    // Create storage image
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(qvkCreateImage(vk.device, &imageInfo, NULL, &vk_compute_rt.storageImage));

    VkMemoryRequirements memReq;
    qvkGetImageMemoryRequirements(vk.device, vk_compute_rt.storageImage, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk_compute_rt.storageImageMemory));
    VK_CHECK(qvkBindImageMemory(vk.device, vk_compute_rt.storageImage, vk_compute_rt.storageImageMemory, 0));

    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vk_compute_rt.storageImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(qvkCreateImageView(vk.device, &viewInfo, NULL, &vk_compute_rt.storageImageView));

    // Create sampler
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VK_CHECK(qvkCreateSampler(vk.device, &samplerInfo, NULL, &vk_compute_rt.storageImageSampler));

    ri.Printf(PRINT_ALL, "Created compute ray tracing storage image: %dx%d\n", width, height);
}

void VK_ComputeRT_AddSphere(const vec3_t position, float radius, const vec3_t diffuse, float specular) {
    if (sceneObjectCount >= MAX_SCENE_OBJECTS) {
        ri.Printf(PRINT_WARNING, "VK_ComputeRT_AddSphere: Maximum scene objects reached\n");
        return;
    }

    rt_scene_object_t* obj = &sceneObjects[sceneObjectCount++];
    obj->properties[0] = position[0];
    obj->properties[1] = position[1];
    obj->properties[2] = position[2];
    obj->properties[3] = radius;
    VectorCopy(diffuse, obj->diffuse);
    obj->specular = specular;
    obj->id = currentObjectId++;
    obj->objectType = RT_OBJECT_SPHERE;
}

void VK_ComputeRT_AddPlane(const vec3_t normal, float distance, const vec3_t diffuse, float specular) {
    if (sceneObjectCount >= MAX_SCENE_OBJECTS) {
        ri.Printf(PRINT_WARNING, "VK_ComputeRT_AddPlane: Maximum scene objects reached\n");
        return;
    }

    rt_scene_object_t* obj = &sceneObjects[sceneObjectCount++];
    obj->properties[0] = normal[0];
    obj->properties[1] = normal[1];
    obj->properties[2] = normal[2];
    obj->properties[3] = distance;
    VectorCopy(diffuse, obj->diffuse);
    obj->specular = specular;
    obj->id = currentObjectId++;
    obj->objectType = RT_OBJECT_PLANE;
}

void VK_ComputeRT_ClearScene(void) {
    sceneObjectCount = 0;
    currentObjectId = 0;
}

void VK_ComputeRT_UpdateCamera(const vec3_t position, const vec3_t lookat, float fov) {
    VectorCopy(position, vk_compute_rt.cameraPos);
    VectorCopy(lookat, vk_compute_rt.cameraLookat);
    vk_compute_rt.cameraFOV = fov;
}

void VK_ComputeRT_UpdateLight(const vec3_t position) {
    VectorCopy(position, vk_compute_rt.lightPos);
}

void VK_ComputeRT_Dispatch(void) {
    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    if (!vk_compute_rt.initialized) return;
    if (!vk_compute_rt.computeShaderLoaded || vk_compute_rt.computePipeline == VK_NULL_HANDLE) {
        if (vk_compute_rt.smokeTestEnabled) {
            // Perform a quick smoke-test render by clearing the storage image to a solid color
            // Ensure we have a command buffer started
            // Insert simple barriers and clear
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.image = vk_compute_rt.storageImage;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(vk_compute_rt.computeCommandBuffer,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, NULL, 0, NULL, 1, &barrier);
            VkClearColorValue clearColor = { {0.0f, 0.0f, 0.0f, 1.0f} };
            VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCmdClearColorImage(vk_compute_rt.computeCommandBuffer, vk_compute_rt.storageImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(vk_compute_rt.computeCommandBuffer,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0, 0, NULL, 0, NULL, 1, &barrier);
            if (vkEndCommandBuffer(vk_compute_rt.computeCommandBuffer) != VK_SUCCESS) {
                ri.Printf(PRINT_ERR, "VK_ComputeRT_Dispatch: failed to end command buffer (smoke)\n");
                return;
            }
            VkSubmitInfo submit = {};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &vk_compute_rt.computeCommandBuffer;
            if (vkQueueSubmit(vk.queue, 1, &submit, vk_compute_rt.computeFence) != VK_SUCCESS) {
                ri.Printf(PRINT_ERR, "VK_ComputeRT_Dispatch: failed to submit smoke test\n");
                return;
            }
            VK_CHECK(vkWaitForFences(vk.device, 1, &vk_compute_rt.computeFence, VK_TRUE, UINT64_MAX));
            vkResetFences(vk.device, 1, &vk_compute_rt.computeFence);
            clock_gettime(CLOCK_MONOTONIC, &ts_end);
            uint64_t duration_ns = (uint64_t)(ts_end.tv_sec - ts_start.tv_sec) * 1000000000ULL + (uint64_t)(ts_end.tv_nsec - ts_start.tv_nsec);
            ri.Printf(PRINT_DEVELOPER, "VK_ComputeRT_Dispatch: smoke test duration %llu ns\n", (unsigned long long)duration_ns);
            #if defined(VK_TIMELINE_SEMAPHORE_ENABLED) || defined(VK_CALIBRATED_TIMESTAMPS_ENABLED)
            vk_update_frame_timing(duration_ns);
            #endif
            ri.Printf(PRINT_DEVELOPER, "VK_ComputeRT_Dispatch: smoke test render complete\n");
            return;
        }
        ri.Printf(PRINT_DEVELOPER, "VK_ComputeRT_Dispatch: compute path not available (no shader)\n");
        return;
    }
    // Begin command buffer for one-time submission
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    // Optional: mark timing start via a timestamp for compute
    if (vk_compute_rt.computeQueryPool != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(vk_compute_rt.computeCommandBuffer, vk_compute_rt.computeQueryPool, 0, 2);
        vkCmdWriteTimestamp(vk_compute_rt.computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk_compute_rt.computeQueryPool, 0);
    }
    if (vkBeginCommandBuffer(vk_compute_rt.computeCommandBuffer, &beginInfo) != VK_SUCCESS) {
        ri.Printf(PRINT_ERR, "VK_ComputeRT_Dispatch: failed to begin command buffer\n");
        return;
    }
    // Optional: stage scene data before dispatch
    size_t dataSize = (size_t)sceneObjectCount * sizeof(rt_scene_object_t);
    if (dataSize > 0 && vk_compute_rt.stagingBufferInitialized) {
        // Map staging memory and copy scene data
        void* mapped = NULL;
        if (vkMapMemory(vk.device, vk_compute_rt.stagingBufferMemory, 0, dataSize, 0, &mapped) == VK_SUCCESS) {
            memcpy(mapped, sceneObjects, dataSize);
            vkUnmapMemory(vk.device, vk_compute_rt.stagingBufferMemory);
        }
        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = dataSize;
        vkCmdCopyBuffer(vk_compute_rt.computeCommandBuffer, vk_compute_rt.stagingBuffer, vk_compute_rt.sceneObjectsBuffer, 1, &copyRegion);
        vk_compute_rt.stagingDataDirty = qfalse;
    } else if (dataSize > 0 && !vk_compute_rt.stagingBufferInitialized) {
        // Initialize a simple staging buffer on first use
        VkBufferCreateInfo sbInfo = {};
        sbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        sbInfo.size = dataSize;
        sbInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        sbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(vk.device, &sbInfo, NULL, &vk_compute_rt.stagingBuffer) == VK_SUCCESS) {
            VkMemoryRequirements memReq;
            vkGetBufferMemoryRequirements(vk.device, vk_compute_rt.stagingBuffer, &memReq);
            VkMemoryAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memReq.size;
            allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if (vkAllocateMemory(vk.device, &allocInfo, NULL, &vk_compute_rt.stagingBufferMemory) == VK_SUCCESS) {
                vkBindBufferMemory(vk.device, vk_compute_rt.stagingBuffer, vk_compute_rt.stagingBufferMemory, 0);
                vk_compute_rt.stagingBufferSize = dataSize;
                vk_compute_rt.stagingBufferInitialized = qtrue;
            }
        }
        // If staging creation failed, we'll skip staging for this frame
    }
    vkCmdBindPipeline(vk_compute_rt.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk_compute_rt.computePipeline);
    vkCmdBindDescriptorSets(vk_compute_rt.computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk_compute_rt.computePipelineLayout, 0, 1, &vk_compute_rt.computeDescriptorSet, 0, NULL);
    uint32_t gx = (uint32_t)((vk_compute_rt.resolution + 15) / 16);
    uint32_t gy = (uint32_t)((vk_compute_rt.resolution + 15) / 16);
    vkCmdDispatch(vk_compute_rt.computeCommandBuffer, gx, gy, 1);
    if (vk_compute_rt.computeQueryPool != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp(vk_compute_rt.computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk_compute_rt.computeQueryPool, 1);
    }
    if (vkEndCommandBuffer(vk_compute_rt.computeCommandBuffer) != VK_SUCCESS) {
        ri.Printf(PRINT_ERR, "VK_ComputeRT_Dispatch: failed to end command buffer\n");
        return;
    }
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &vk_compute_rt.computeCommandBuffer;
    if (vkQueueSubmit(vk.queue, 1, &submit, vk_compute_rt.computeFence) != VK_SUCCESS) {
        ri.Printf(PRINT_ERR, "VK_ComputeRT_Dispatch: failed to submit compute work\n");
        return;
    }
    VK_CHECK(vkWaitForFences(vk.device, 1, &vk_compute_rt.computeFence, VK_TRUE, UINT64_MAX));
    vkResetFences(vk.device, 1, &vk_compute_rt.computeFence);
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    uint64_t duration_ns = (uint64_t)(ts_end.tv_sec - ts_start.tv_sec) * 1000000000ULL + (uint64_t)(ts_end.tv_nsec - ts_start.tv_nsec);
    ri.Printf(PRINT_DEVELOPER, "VK_ComputeRT_Dispatch: duration %llu ns\n", (unsigned long long)duration_ns);
    // Retrieve GPU timestamps if available and convert to nanoseconds
    if (vk_compute_rt.computeQueryPool != VK_NULL_HANDLE) {
        uint64_t timestamps[2] = {0, 0};
        VkResult res = vkGetQueryPoolResults(vk.device, vk_compute_rt.computeQueryPool, 0, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
        if (res == VK_SUCCESS) {
            uint64_t gpu_duration_ns = (timestamps[1] - timestamps[0]) * (uint64_t)vk_compute_rt.timestampPeriod;
            ri.Printf(PRINT_DEVELOPER, "VK_ComputeRT_Dispatch: gpu duration %llu ns\n", (unsigned long long)gpu_duration_ns);
            #if defined(VK_TIMELINE_SEMAPHORE_ENABLED) || defined(VK_CALIBRATED_TIMESTAMPS_ENABLED)
            vk_update_frame_timing(gpu_duration_ns);
            #endif
        }
    }
#if defined(VK_TIMELINE_SEMAPHORE_ENABLED) || defined(VK_CALIBRATED_TIMESTAMPS_ENABLED)
    vk_update_frame_timing(duration_ns);
#endif
    ri.Printf(PRINT_DEVELOPER, "VK_ComputeRT_Dispatch: compute path dispatched\n");
}

void VK_ComputeRT_RenderFullscreen(void) {
    // Render fullscreen quad with ray traced image
    // This would be integrated into the main rendering pipeline
    // Placeholder for now
}

qboolean VK_ComputeRT_IsEnabled(void) {
    return vk_compute_rt.enabled && vk_compute_rt.initialized;
}

void VK_ComputeRT_SetEnabled(qboolean enabled) {
    vk_compute_rt.enabled = enabled;
}

void VK_ComputeRT_SetResolution(int resolution) {
    vk_compute_rt.resolution = Com_Clamp(256, 4096, resolution);
    // Would recreate storage image here
}

void VK_ComputeRT_SetMaxBounces(int bounces) {
    vk_compute_rt.maxBounces = Com_Clamp(0, 8, bounces);
}

void VK_ComputeRT_SetUseReflections(qboolean use) {
    vk_compute_rt.useReflections = use;
}

void VK_ComputeRT_ReloadShader(void) {
    // Unload existing shader if any
    if (vk_compute_rt.computeShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(vk.device, vk_compute_rt.computeShaderModule, NULL);
        vk_compute_rt.computeShaderModule = VK_NULL_HANDLE;
        vk_compute_rt.computeShaderLoaded = qfalse;
    }
    // Attempt to load shader again (same path as Init)
    const char* shaderPath = "./shaders/compute_raytracing.comp.spv";
    FILE* shaderFile = fopen(shaderPath, "rb");
    if (shaderFile) {
        fseek(shaderFile, 0, SEEK_END);
        long shaderSize = ftell(shaderFile);
        rewind(shaderFile);
        if (shaderSize > 0) {
            uint32_t* shaderCode = (uint32_t*)malloc(shaderSize);
            if (shaderCode && fread(shaderCode, 1, shaderSize, shaderFile) == (size_t)shaderSize) {
                VkShaderModuleCreateInfo smci = {};
                smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                smci.codeSize = (size_t)shaderSize;
                smci.pCode = shaderCode;
                if (vkCreateShaderModule(vk.device, &smci, NULL, &vk_compute_rt.computeShaderModule) == VK_SUCCESS) {
                    vk_compute_rt.computeShaderLoaded = qtrue;
                    ri.Printf(PRINT_ALL, "VK_ComputeRT_ReloadShader: loaded shader module (%s)\n", shaderPath);
                    // Try to create pipeline if layout exists and not yet created
                    if (vk_compute_rt.computePipeline == VK_NULL_HANDLE) {
                        VkPipelineShaderStageCreateInfo stage = {};
                        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                        stage.module = vk_compute_rt.computeShaderModule;
                        stage.pName = "main";
                        VkComputePipelineCreateInfo cpi = {};
                        cpi.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                        cpi.stage = stage;
                        cpi.layout = vk_compute_rt.computePipelineLayout;
                        if (vkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &cpi, NULL, &vk_compute_rt.computePipeline) == VK_SUCCESS) {
                            ri.Printf(PRINT_ALL, "VK_ComputeRT_ReloadShader: compute pipeline created\n");
                        } else {
                            ri.Printf(PRINT_WARNING, "VK_ComputeRT_ReloadShader: failed to create compute pipeline\n");
                        }
                    }
                } else {
                    ri.Printf(PRINT_WARNING, "VK_ComputeRT_ReloadShader: failed to create shader module\n");
                }
            } else {
                ri.Printf(PRINT_WARNING, "VK_ComputeRT_ReloadShader: failed to read shader file\n");
            }
            free(shaderCode);
        } else {
            ri.Printf(PRINT_WARNING, "VK_ComputeRT_ReloadShader: shader file empty\n");
        }
        fclose(shaderFile);
    } else {
        ri.Printf(PRINT_WARNING, "VK_ComputeRT_ReloadShader: shader not found: %s\n", shaderPath);
    }
}

// Enable/disable a CPU-side smoke-test render path when no shader is available.
void VK_ComputeRT_EnableSmokeTest(qboolean enabled) {
    vk_compute_rt.smokeTestEnabled = enabled;
    if (enabled) {
        ri.Printf(PRINT_ALL, "VK_ComputeRT: smoke-test mode enabled (CPU-based frame render)\n");
    } else {
        ri.Printf(PRINT_ALL, "VK_ComputeRT: smoke-test mode disabled\n");
    }
}

// Small wrapper to render a single frame batch (update data, then dispatch)
void VK_ComputeRT_BatchRenderFrame(void) {
    // This wrapper allows external code to trigger a full compute-frame sequence
    // For now, simply invoke the main dispatch path (which handles staging)
    VK_ComputeRT_Dispatch();
}
