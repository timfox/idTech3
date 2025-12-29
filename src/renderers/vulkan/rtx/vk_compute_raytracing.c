#include "vk_compute_raytracing.h"
#include "vk.h"
#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "../../common/qcommon.h"
#include <string.h>
#include <math.h>

vk_compute_rt_t vk_compute_rt;

// Scene objects storage (temporary, will be uploaded to GPU)
#define MAX_SCENE_OBJECTS 256
static rt_scene_object_t sceneObjects[MAX_SCENE_OBJECTS];
static int sceneObjectCount = 0;
static uint32_t currentObjectId = 0;

void VK_ComputeRT_Init(void) {
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

    ri.Printf(PRINT_ALL, "Compute ray tracing system initialized\n");
}

void VK_ComputeRT_Shutdown(void) {
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
    if (!vk_compute_rt.enabled || !vk_compute_rt.initialized) return;

    // Update uniform buffer
    if (vk_compute_rt.uniformBufferMapped) {
        float* uniforms = (float*)vk_compute_rt.uniformBufferMapped;
        
        // Light position
        uniforms[0] = vk_compute_rt.lightPos[0];
        uniforms[1] = vk_compute_rt.lightPos[1];
        uniforms[2] = vk_compute_rt.lightPos[2];
        
        // Aspect ratio
        uniforms[3] = vk_compute_rt.aspectRatio;
        
        // Fog color
        uniforms[4] = vk_compute_rt.fogColor[0];
        uniforms[5] = vk_compute_rt.fogColor[1];
        uniforms[6] = vk_compute_rt.fogColor[2];
        uniforms[7] = vk_compute_rt.fogColor[3];
        
        // Camera position
        uniforms[8] = vk_compute_rt.cameraPos[0];
        uniforms[9] = vk_compute_rt.cameraPos[1];
        uniforms[10] = vk_compute_rt.cameraPos[2];
        
        // Camera lookat
        uniforms[11] = vk_compute_rt.cameraLookat[0];
        uniforms[12] = vk_compute_rt.cameraLookat[1];
        uniforms[13] = vk_compute_rt.cameraLookat[2];
        
        // Camera FOV
        uniforms[14] = vk_compute_rt.cameraFOV;
    }

    // Upload scene objects to GPU (would use staging buffer in full implementation)
    // For now, this is a placeholder

    // Compute shader dispatch would happen here
    // This is a placeholder - full implementation would:
    // 1. Build compute command buffer
    // 2. Bind pipeline and descriptor sets
    // 3. Dispatch compute shader
    // 4. Synchronize with graphics queue
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
