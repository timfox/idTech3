#include "vk_sem.h"
#include "vk.h"
#include "tr_local.h"
#include "../../qcommon/qcommon.h"
#include <string.h>

vk_sem_t vk_sem;

void VK_SEM_Init(void) {
    memset(&vk_sem, 0, sizeof(vk_sem_t));

    // Create descriptor set layout
    VkDescriptorSetLayoutBinding bindings[2] = {};

    // Uniform buffer
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // Mat cap array texture
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk_sem.descriptorSetLayout));

    // Create uniform buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(float) * 16 * 2 + sizeof(int32_t); // projection, model, normal, view matrices + matCapIndex
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(qvkCreateBuffer(vk.device, &bufferInfo, NULL, &vk_sem.uniformBuffer));

    VkMemoryRequirements memReq;
    qvkGetBufferMemoryRequirements(vk.device, vk_sem.uniformBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk_sem.uniformBufferMemory));
    VK_CHECK(qvkBindBufferMemory(vk.device, vk_sem.uniformBuffer, vk_sem.uniformBufferMemory, 0));

    VK_CHECK(qvkMapMemory(vk.device, vk_sem.uniformBufferMemory, 0, bufferInfo.size, 0, &vk_sem.uniformBufferMapped));

    // Create sampler for mat cap array
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    VK_CHECK(qvkCreateSampler(vk.device, &samplerInfo, NULL, &vk_sem.matCapArraySampler));

    vk_sem.currentMatCapIndex = 0;
    vk_sem.intensity = 1.0f;
    vk_sem.useNormalMap = qfalse;
    vk_sem.enabled = qtrue;
    vk_sem.initialized = qtrue;

    ri.Printf(PRINT_ALL, "SEM system initialized\n");
}

void VK_SEM_Shutdown(void) {
    if (!vk_sem.initialized) return;

    if (vk_sem.uniformBufferMapped) {
        qvkUnmapMemory(vk.device, vk_sem.uniformBufferMemory);
    }
    if (vk_sem.uniformBuffer != VK_NULL_HANDLE) {
        qvkDestroyBuffer(vk.device, vk_sem.uniformBuffer, NULL);
    }
    if (vk_sem.uniformBufferMemory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk_sem.uniformBufferMemory, NULL);
    }

    if (vk_sem.matCapArraySampler != VK_NULL_HANDLE) {
        qvkDestroySampler(vk.device, vk_sem.matCapArraySampler, NULL);
    }
    if (vk_sem.matCapArrayView != VK_NULL_HANDLE) {
        qvkDestroyImageView(vk.device, vk_sem.matCapArrayView, NULL);
    }
    if (vk_sem.matCapArrayImage != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk_sem.matCapArrayImage, NULL);
    }
    if (vk_sem.matCapArrayMemory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk_sem.matCapArrayMemory, NULL);
    }

    if (vk_sem.descriptorSetLayout != VK_NULL_HANDLE) {
        qvkDestroyDescriptorSetLayout(vk.device, vk_sem.descriptorSetLayout, NULL);
    }

    memset(&vk_sem, 0, sizeof(vk_sem_t));
}

void VK_SEM_LoadMatCapArray(const char* filename) {
    // Load mat cap array texture
    // This would load a KTX texture array file
    // For now, create a placeholder texture array

    char matCapPath[MAX_QPATH];
    Com_sprintf(matCapPath, sizeof(matCapPath), "textures/%s", filename);

    // Try to load the texture using the engine's texture loading system
    // For now, we'll create a simple placeholder
    // In a full implementation, this would use R_LoadImage or similar

    ri.Printf(PRINT_ALL, "SEM: Loading mat cap array from %s\n", matCapPath);
    // TODO: Implement actual texture array loading
    // This would involve:
    // 1. Loading KTX file with array layers
    // 2. Creating VkImage with VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT
    // 3. Creating VkImageView with VK_IMAGE_VIEW_TYPE_2D_ARRAY
    // 4. Updating descriptor set

    vk_sem.matCapLayerCount = 1; // Placeholder
}

void VK_SEM_SetMatCapIndex(int index) {
    if (index < 0) index = 0;
    if (index >= vk_sem.matCapLayerCount) index = vk_sem.matCapLayerCount - 1;
    vk_sem.currentMatCapIndex = index;
}

int VK_SEM_GetMatCapCount(void) {
    return vk_sem.matCapLayerCount;
}

void VK_SEM_UpdateUniforms(const float* viewMatrix, const float* modelMatrix) {
    if (!vk_sem.initialized || !vk_sem.uniformBufferMapped) return;

    // Update uniform buffer with matrices and mat cap index
    // Layout: projection (16 floats), model (16 floats), normal (16 floats), view (16 floats), matCapIndex (1 int)
    float* uniforms = (float*)vk_sem.uniformBufferMapped;
    
    // Copy matrices (would be passed in from renderer)
    // For now, just update the mat cap index
    int32_t* matCapIndexPtr = (int32_t*)(uniforms + 64); // After 4 matrices (16 floats each)
    *matCapIndexPtr = vk_sem.currentMatCapIndex;
}

void VK_SEM_BindDescriptors(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) {
    if (!vk_sem.initialized || vk_sem.descriptorSet == VK_NULL_HANDLE) return;

    qvkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout, 0, 1, &vk_sem.descriptorSet, 0, NULL);
}

qboolean VK_SEM_IsEnabled(void) {
    return vk_sem.enabled && vk_sem.initialized;
}

void VK_SEM_SetIntensity(float intensity) {
    vk_sem.intensity = Com_Clamp(0.0f, 2.0f, intensity);
}

void VK_SEM_SetUseNormalMap(qboolean useNormalMap) {
    vk_sem.useNormalMap = useNormalMap;
}
