#include "vk_sem.h"
#include "vk.h"
#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "../../common/qcommon.h"
#include <string.h>

// Forward declarations
extern uint32_t find_memory_type(uint32_t memory_type_bits, VkMemoryPropertyFlags properties);
extern const char *vk_result_string(VkResult result);
extern Vk_Instance vk;

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
    if (!vk_sem.initialized) {
        ri.Printf(PRINT_WARNING, "SEM: Cannot load mat cap array - SEM system not initialized\n");
        return;
    }

    // Clean up existing mat cap array if it exists
    if (vk_sem.matCapArrayView != VK_NULL_HANDLE) {
        qvkDestroyImageView(vk.device, vk_sem.matCapArrayView, NULL);
        vk_sem.matCapArrayView = VK_NULL_HANDLE;
    }
    if (vk_sem.matCapArrayImage != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk_sem.matCapArrayImage, NULL);
        vk_sem.matCapArrayImage = VK_NULL_HANDLE;
    }
    if (vk_sem.matCapArrayMemory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk_sem.matCapArrayMemory, NULL);
        vk_sem.matCapArrayMemory = VK_NULL_HANDLE;
    }

    char matCapPath[MAX_QPATH];
    Com_sprintf(matCapPath, sizeof(matCapPath), "textures/%s", filename);

    ri.Printf(PRINT_ALL, "SEM: Loading mat cap array from %s\n", matCapPath);

    // Try to load as a sequence of PNG files (matcap_00.png, matcap_01.png, etc.)
    // or as a single KTX array file
    extern void R_LoadPNG(const char *name, byte **pic, int *width, int *height);
    
    const int MAX_MATCAP_LAYERS = 64;
    byte *pics[MAX_MATCAP_LAYERS] = {NULL};
    int widths[MAX_MATCAP_LAYERS] = {0};
    int heights[MAX_MATCAP_LAYERS] = {0};
    int loaded_count = 0;
    int expected_width = 0, expected_height = 0;

    // Try loading numbered sequence first (matcap_00.png, matcap_01.png, ...)
    for (int i = 0; i < MAX_MATCAP_LAYERS; i++) {
        char layerPath[MAX_QPATH];
        Com_sprintf(layerPath, sizeof(layerPath), "%s_%02d.png", matCapPath, i);
        
        R_LoadPNG(layerPath, &pics[loaded_count], &widths[loaded_count], &heights[loaded_count]);
        if (pics[loaded_count] == NULL) {
            // Try without leading zeros
            Com_sprintf(layerPath, sizeof(layerPath), "%s_%d.png", matCapPath, i);
            R_LoadPNG(layerPath, &pics[loaded_count], &widths[loaded_count], &heights[loaded_count]);
        }
        
        if (pics[loaded_count] == NULL) {
            break; // No more layers found
        }

        // Verify all layers have the same dimensions
        if (loaded_count == 0) {
            expected_width = widths[0];
            expected_height = heights[0];
        } else if (widths[loaded_count] != expected_width || heights[loaded_count] != expected_height) {
            ri.Printf(PRINT_WARNING, "SEM: Mat cap layer %d has wrong dimensions (%dx%d, expected %dx%d), stopping\n",
                i, widths[loaded_count], heights[loaded_count], expected_width, expected_height);
            ri.Free(pics[loaded_count]);
            break;
        }

        loaded_count++;
    }

    // If no numbered sequence found, try loading as single file
    if (loaded_count == 0) {
        R_LoadPNG(matCapPath, &pics[0], &widths[0], &heights[0]);
        if (pics[0] != NULL) {
            loaded_count = 1;
            expected_width = widths[0];
            expected_height = heights[0];
        }
    }

    if (loaded_count == 0) {
        ri.Printf(PRINT_WARNING, "SEM: Failed to load mat cap array from %s\n", matCapPath);
        vk_sem.matCapLayerCount = 0;
        return;
    }

    // Create Vulkan image with array layers
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width = expected_width;
    imageInfo.extent.height = expected_height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = loaded_count;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.flags = 0; // VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT not needed for 2D arrays

    VkResult result = qvkCreateImage(vk.device, &imageInfo, NULL, &vk_sem.matCapArrayImage);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "SEM: Failed to create mat cap array image: %s\n", vk_result_string(result));
        for (int i = 0; i < loaded_count; i++) {
            ri.Free(pics[i]);
        }
        vk_sem.matCapLayerCount = 0;
        return;
    }

    // Allocate and bind memory
    VkMemoryRequirements memRequirements;
    qvkGetImageMemoryRequirements(vk.device, vk_sem.matCapArrayImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    result = qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk_sem.matCapArrayMemory);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "SEM: Failed to allocate mat cap array memory: %s\n", vk_result_string(result));
        qvkDestroyImage(vk.device, vk_sem.matCapArrayImage, NULL);
        vk_sem.matCapArrayImage = VK_NULL_HANDLE;
        for (int i = 0; i < loaded_count; i++) {
            ri.Free(pics[i]);
        }
        vk_sem.matCapLayerCount = 0;
        return;
    }

    qvkBindImageMemory(vk.device, vk_sem.matCapArrayImage, vk_sem.matCapArrayMemory, 0);

    // Create image view for 2D array
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vk_sem.matCapArrayImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = loaded_count;

    result = qvkCreateImageView(vk.device, &viewInfo, NULL, &vk_sem.matCapArrayView);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "SEM: Failed to create mat cap array image view: %s\n", vk_result_string(result));
        qvkFreeMemory(vk.device, vk_sem.matCapArrayMemory, NULL);
        qvkDestroyImage(vk.device, vk_sem.matCapArrayImage, NULL);
        vk_sem.matCapArrayMemory = VK_NULL_HANDLE;
        vk_sem.matCapArrayImage = VK_NULL_HANDLE;
        for (int i = 0; i < loaded_count; i++) {
            ri.Free(pics[i]);
        }
        vk_sem.matCapLayerCount = 0;
        return;
    }

    // Upload texture data using staging buffer
    // Note: This is a simplified upload - in production, you'd want proper command buffer handling
    int pixel_size = expected_width * expected_height * 4; // RGBA
    int total_size = pixel_size * loaded_count;

    // Use staging buffer to upload data
    if (vk.staging_buffer.size >= (size_t)total_size && vk.staging_buffer.ptr != NULL) {
        byte *staging_ptr = (byte*)vk.staging_buffer.ptr;
        for (int i = 0; i < loaded_count; i++) {
            Com_Memcpy(staging_ptr, pics[i], pixel_size);
            staging_ptr += pixel_size;
            ri.Free(pics[i]);
        }

        // Transition image layout and copy from staging buffer
        // This would normally be done via command buffer, but for now we'll
        // rely on the existing staging buffer upload mechanism
        // The actual upload would be handled by vk_upload_image_data or similar
        ri.Printf(PRINT_DEVELOPER, "SEM: Mat cap array data staged, upload via command buffer needed\n");
    } else {
        ri.Printf(PRINT_WARNING, "SEM: Staging buffer too small (%zu < %d) or unavailable, skipping upload\n",
            vk.staging_buffer.size, total_size);
        for (int i = 0; i < loaded_count; i++) {
            ri.Free(pics[i]);
        }
    }

    vk_sem.matCapLayerCount = loaded_count;
    vk_sem.currentMatCapIndex = 0;

    ri.Printf(PRINT_ALL, "SEM: Loaded mat cap array with %d layers (%dx%d each)\n",
        loaded_count, expected_width, expected_height);
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
