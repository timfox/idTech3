<?php
/**
 * Resource Management - Vulkan Memory and Resource Lifecycle
 */
$title = 'Resource Management - id Tech 3 Documentation';
$breadcrumbs = [
    '/renderer' => 'Renderer Deep Dive',
    '/renderer/resource-management' => 'Resource Management'
];
?>

<h1>Resource Management - Vulkan Memory and Resource Lifecycle</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Efficient resource management is critical for Vulkan applications. JKSunny's PBR port implements sophisticated memory management using the Vulkan Memory Allocator (VMA) along with custom pooling strategies for optimal performance and memory usage.</p>
    
    <div class="feature-list">
        <h3>Resource Management Features</h3>
        <ul>
            <li><strong>VMA Integration:</strong> Vulkan Memory Allocator for efficient memory management</li>
            <li><strong>Staging Buffers:</strong> Optimized data upload to GPU memory</li>
            <li><strong>Resource Pooling:</strong> Reusable buffer and image pools</li>
            <li><strong>Lifecycle Management:</strong> Automatic cleanup and garbage collection</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Vulkan Memory Allocator Integration</h2>
    
    <h3>VMA Initialization and Configuration</h3>
    <div class="code-block">
        <pre><code>// tr_vma.c - Vulkan Memory Allocator integration
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

typedef struct vmaContext_s {
    VmaAllocator allocator;
    VmaAllocatorCreateInfo allocatorInfo;
    
    // Memory pools for different usage patterns
    VmaPool bufferPool;         // General buffer allocations
    VmaPool imagePool;          // Texture and render target allocations
    VmaPool stagingPool;        // Staging buffer allocations
    VmaPool uniformPool;        // Uniform buffer allocations
    
    // Statistics
    VmaStats stats;
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    
} vmaContext_t;

static vmaContext_t vma;

qboolean VMA_Init(void) {
    Com_Printf("Initializing Vulkan Memory Allocator\n");
    
    // VMA allocator creation
    vma.allocatorInfo = (VmaAllocatorCreateInfo){
        .flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
        .physicalDevice = vk.physicalDevice,
        .device = vk.device,
        .instance = vk.instance,
        .vulkanApiVersion = VK_API_VERSION_1_2,
    };
    
    VkResult result = vmaCreateAllocator(&vma.allocatorInfo, &vma.allocator);
    if (result != VK_SUCCESS) {
        Com_Printf("^1Failed to create VMA allocator: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    // Create memory pools for different usage patterns
    if (!VMA_CreateMemoryPools()) {
        Com_Printf("^1Failed to create VMA memory pools\n");
        return qfalse;
    }
    
    // Print memory information
    VMA_PrintMemoryInfo();
    
    return qtrue;
}

qboolean VMA_CreateMemoryPools(void) {
    // General buffer pool
    VmaPoolCreateInfo bufferPoolInfo = {
        .memoryTypeIndex = VMA_FindMemoryTypeIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        .blockSize = 64 * 1024 * 1024,  // 64MB blocks
        .minBlockCount = 1,
        .maxBlockCount = 8,
    };
    
    VkResult result = vmaCreatePool(vma.allocator, &bufferPoolInfo, &vma.bufferPool);
    if (result != VK_SUCCESS) {
        Com_Printf("^1Failed to create buffer pool: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    // Image pool for textures
    VmaPoolCreateInfo imagePoolInfo = {
        .memoryTypeIndex = VMA_FindMemoryTypeIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        .blockSize = 128 * 1024 * 1024,  // 128MB blocks for textures
        .minBlockCount = 1,
        .maxBlockCount = 16,
    };
    
    result = vmaCreatePool(vma.allocator, &imagePoolInfo, &vma.imagePool);
    if (result != VK_SUCCESS) {
        Com_Printf("^1Failed to create image pool: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    // Staging buffer pool (host visible)
    VmaPoolCreateInfo stagingPoolInfo = {
        .memoryTypeIndex = VMA_FindMemoryTypeIndex(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        .blockSize = 32 * 1024 * 1024,  // 32MB blocks
        .minBlockCount = 1,
        .maxBlockCount = 4,
    };
    
    result = vmaCreatePool(vma.allocator, &stagingPoolInfo, &vma.stagingPool);
    if (result != VK_SUCCESS) {
        Com_Printf("^1Failed to create staging pool: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    // Uniform buffer pool (host visible, coherent)
    VmaPoolCreateInfo uniformPoolInfo = {
        .memoryTypeIndex = VMA_FindMemoryTypeIndex(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        .blockSize = 16 * 1024 * 1024,  // 16MB blocks
        .minBlockCount = 1,
        .maxBlockCount = 2,
    };
    
    result = vmaCreatePool(vma.allocator, &uniformPoolInfo, &vma.uniformPool);
    if (result != VK_SUCCESS) {
        Com_Printf("^1Failed to create uniform pool: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    return qtrue;
}

uint32_t VMA_FindMemoryTypeIndex(VkMemoryPropertyFlags requiredFlags) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(vk.physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (memProperties.memoryTypes[i].propertyFlags & requiredFlags) {
            return i;
        }
    }
    
    Com_Error(ERR_FATAL, "Failed to find suitable memory type");
    return 0;
}

void VMA_PrintMemoryInfo(void) {
    vmaCalculateStats(vma.allocator, &vma.stats);
    
    Com_Printf("VMA Memory Statistics:\n");
    Com_Printf("  Total allocated: %zu MB\n", vma.stats.total.usedBytes / (1024 * 1024));
    Com_Printf("  Total unused: %zu MB\n", vma.stats.total.unusedBytes / (1024 * 1024));
    Com_Printf("  Allocation count: %u\n", vma.stats.total.allocationCount);
    Com_Printf("  Block count: %u\n", vma.stats.total.blockCount);
    
    // Print budget information
    vmaGetBudget(vma.allocator, vma.budgets);
    for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; i++) {
        if (vma.budgets[i].budget > 0) {
            Com_Printf("  Heap %u: %zu MB / %zu MB (%.1f%%)\n", i,
                      vma.budgets[i].usage / (1024 * 1024),
                      vma.budgets[i].budget / (1024 * 1024),
                      (float)vma.budgets[i].usage / vma.budgets[i].budget * 100.0f);
        }
    }
}</code></pre>
    </div>
    
    <h3>Buffer Management</h3>
    <div class="code-block">
        <pre><code>// Buffer creation and management with VMA
typedef struct vkBuffer_s {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocInfo;
    
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VmaMemoryUsage memoryUsage;
    
    void* mapped;           // Persistent mapping for host-visible buffers
    qboolean persistentlyMapped;
    
    int refCount;
    int lastUsedFrame;
    
} vkBuffer_t;

qboolean VK_CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                        VmaMemoryUsage memoryUsage, vkBuffer_t* vkBuffer) {
    
    // Buffer creation info
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    
    // VMA allocation info
    VmaAllocationCreateInfo allocInfo = {
        .usage = memoryUsage,
    };
    
    // Select appropriate pool based on usage
    switch (memoryUsage) {
    case VMA_MEMORY_USAGE_GPU_ONLY:
        allocInfo.pool = vma.bufferPool;
        break;
    case VMA_MEMORY_USAGE_CPU_TO_GPU:
        allocInfo.pool = vma.stagingPool;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;
    case VMA_MEMORY_USAGE_CPU_ONLY:
        allocInfo.pool = vma.uniformPool;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;
    }
    
    VkResult result = vmaCreateBuffer(vma.allocator, &bufferInfo, &allocInfo,
                                     &vkBuffer->buffer, &vkBuffer->allocation,
                                     &vkBuffer->allocInfo);
    
    if (result != VK_SUCCESS) {
        Com_Printf("^1vmaCreateBuffer failed: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    vkBuffer->size = size;
    vkBuffer->usage = usage;
    vkBuffer->memoryUsage = memoryUsage;
    vkBuffer->refCount = 1;
    vkBuffer->lastUsedFrame = vk.currentFrame;
    
    // Set up persistent mapping for host-visible buffers
    if (memoryUsage != VMA_MEMORY_USAGE_GPU_ONLY) {
        vkBuffer->mapped = vkBuffer->allocInfo.pMappedData;
        vkBuffer->persistentlyMapped = qtrue;
    }
    
    return qtrue;
}

void VK_DestroyBuffer(vkBuffer_t* vkBuffer) {
    if (!vkBuffer || !vkBuffer->buffer) {
        return;
    }
    
    // Unmap if not persistently mapped
    if (vkBuffer->mapped && !vkBuffer->persistentlyMapped) {
        vmaUnmapMemory(vma.allocator, vkBuffer->allocation);
    }
    
    vmaDestroyBuffer(vma.allocator, vkBuffer->buffer, vkBuffer->allocation);
    memset(vkBuffer, 0, sizeof(vkBuffer_t));
}

qboolean VK_MapBuffer(vkBuffer_t* vkBuffer, void** data) {
    if (vkBuffer->persistentlyMapped) {
        *data = vkBuffer->mapped;
        return qtrue;
    }
    
    VkResult result = vmaMapMemory(vma.allocator, vkBuffer->allocation, data);
    if (result != VK_SUCCESS) {
        Com_Printf("^1vmaMapMemory failed: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    vkBuffer->mapped = *data;
    return qtrue;
}

void VK_UnmapBuffer(vkBuffer_t* vkBuffer) {
    if (vkBuffer->persistentlyMapped) {
        return; // Don't unmap persistently mapped buffers
    }
    
    if (vkBuffer->mapped) {
        vmaUnmapMemory(vma.allocator, vkBuffer->allocation);
        vkBuffer->mapped = NULL;
    }
}

// Staging buffer management for efficient uploads
typedef struct stagingBuffer_s {
    vkBuffer_t buffer;
    VkDeviceSize offset;
    VkDeviceSize capacity;
    qboolean inUse;
} stagingBuffer_t;

#define MAX_STAGING_BUFFERS 8
#define STAGING_BUFFER_SIZE (16 * 1024 * 1024)  // 16MB

static stagingBuffer_t stagingBuffers[MAX_STAGING_BUFFERS];
static int numStagingBuffers = 0;

stagingBuffer_t* VK_GetStagingBuffer(VkDeviceSize size) {
    // Try to find an existing buffer with enough space
    for (int i = 0; i < numStagingBuffers; i++) {
        stagingBuffer_t* staging = &stagingBuffers[i];
        
        if (!staging->inUse && 
            staging->capacity - staging->offset >= size) {
            staging->inUse = qtrue;
            return staging;
        }
    }
    
    // Create new staging buffer if needed
    if (numStagingBuffers < MAX_STAGING_BUFFERS) {
        stagingBuffer_t* staging = &stagingBuffers[numStagingBuffers++];
        
        VkDeviceSize bufferSize = max(size, STAGING_BUFFER_SIZE);
        if (!VK_CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VMA_MEMORY_USAGE_CPU_TO_GPU, &staging->buffer)) {
            Com_Printf("^1Failed to create staging buffer\n");
            return NULL;
        }
        
        staging->offset = 0;
        staging->capacity = bufferSize;
        staging->inUse = qtrue;
        
        return staging;
    }
    
    Com_Printf("^3Warning: No available staging buffers\n");
    return NULL;
}

void VK_ReleaseStagingBuffer(stagingBuffer_t* staging) {
    if (staging) {
        staging->inUse = qfalse;
        staging->offset = 0;  // Reset for next use
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Image and Texture Management</h2>
    
    <h3>Image Creation and Layout Transitions</h3>
    <div class="code-block">
        <pre><code>// Image and texture management with VMA
typedef struct vkImage_s {
    VkImage image;
    VkImageView view;
    VmaAllocation allocation;
    VmaAllocationInfo allocInfo;
    
    VkFormat format;
    VkExtent3D extent;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    VkImageUsageFlags usage;
    VkImageLayout currentLayout;
    
    int refCount;
    int lastUsedFrame;
    
} vkImage_t;

qboolean VK_CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                       VkFormat format, VkImageUsageFlags usage, 
                       VmaMemoryUsage memoryUsage, vkImage_t* vkImage) {
    
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = width,
        .extent.height = height,
        .extent.depth = 1,
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .format = format,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = usage,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    
    VmaAllocationCreateInfo allocInfo = {
        .usage = memoryUsage,
        .pool = vma.imagePool,
    };
    
    VkResult result = vmaCreateImage(vma.allocator, &imageInfo, &allocInfo,
                                    &vkImage->image, &vkImage->allocation,
                                    &vkImage->allocInfo);
    
    if (result != VK_SUCCESS) {
        Com_Printf("^1vmaCreateImage failed: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    // Store image properties
    vkImage->format = format;
    vkImage->extent.width = width;
    vkImage->extent.height = height;
    vkImage->extent.depth = 1;
    vkImage->mipLevels = mipLevels;
    vkImage->arrayLayers = 1;
    vkImage->usage = usage;
    vkImage->currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkImage->refCount = 1;
    vkImage->lastUsedFrame = vk.currentFrame;
    
    return qtrue;
}

qboolean VK_CreateImageView(vkImage_t* vkImage, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = vkImage->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = vkImage->format,
        .subresourceRange.aspectMask = aspectFlags,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = vkImage->mipLevels,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = vkImage->arrayLayers,
    };
    
    VkResult result = vkCreateImageView(vk.device, &viewInfo, NULL, &vkImage->view);
    if (result != VK_SUCCESS) {
        Com_Printf("^1vkCreateImageView failed: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    return qtrue;
}

void VK_TransitionImageLayout(VkCommandBuffer commandBuffer, vkImage_t* vkImage,
                             VkImageLayout newLayout) {
    
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = vkImage->currentLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vkImage->image,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = vkImage->mipLevels,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = vkImage->arrayLayers,
    };
    
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;
    
    // Determine access masks and pipeline stages
    if (vkImage->currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        
    } else if (vkImage->currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && 
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        
    } else if (vkImage->currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
               newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | 
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        
    } else {
        Com_Error(ERR_FATAL, "Unsupported layout transition");
    }
    
    vkCmdPipelineBarrier(commandBuffer,
                        sourceStage, destinationStage,
                        0,
                        0, NULL,
                        0, NULL,
                        1, &barrier);
    
    vkImage->currentLayout = newLayout;
}

void VK_CopyBufferToImage(VkCommandBuffer commandBuffer, vkBuffer_t* buffer, 
                         vkImage_t* image) {
    
    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .imageSubresource.mipLevel = 0,
        .imageSubresource.baseArrayLayer = 0,
        .imageSubresource.layerCount = 1,
        .imageOffset = {0, 0, 0},
        .imageExtent = image->extent,
    };
    
    vkCmdCopyBufferToImage(commandBuffer, buffer->buffer, image->image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void VK_DestroyImage(vkImage_t* vkImage) {
    if (!vkImage || !vkImage->image) {
        return;
    }
    
    if (vkImage->view) {
        vkDestroyImageView(vk.device, vkImage->view, NULL);
    }
    
    vmaDestroyImage(vma.allocator, vkImage->image, vkImage->allocation);
    memset(vkImage, 0, sizeof(vkImage_t));
}</code></pre>
    </div>
    
    <h3>Texture Loading and Mipmap Generation</h3>
    <div class="code-block">
        <pre><code>// High-level texture loading with staging and mipmap generation
typedef struct texture_s {
    char name[MAX_QPATH];
    vkImage_t image;
    VkSampler sampler;
    
    int width, height;
    int mipLevels;
    VkFormat format;
    
    int flags;
    #define TEXTURE_FLAG_NOMIPMAP    (1 << 0)
    #define TEXTURE_FLAG_NOPICMIP    (1 << 1)
    #define TEXTURE_FLAG_CLAMP       (1 << 2)
    
    int frameUsed;
    
} texture_t;

#define MAX_TEXTURES 2048
static texture_t textures[MAX_TEXTURES];
static int numTextures = 0;

texture_t* R_LoadTextureVK(const char* name, int flags) {
    // Check if already loaded
    for (int i = 0; i < numTextures; i++) {
        if (!strcmp(textures[i].name, name)) {
            textures[i].frameUsed = vk.currentFrame;
            return &textures[i];
        }
    }
    
    if (numTextures >= MAX_TEXTURES) {
        Com_Printf("^1R_LoadTextureVK: MAX_TEXTURES exceeded\n");
        return NULL;
    }
    
    texture_t* texture = &textures[numTextures++];
    memset(texture, 0, sizeof(texture_t));
    Q_strncpyz(texture->name, name, sizeof(texture->name));
    texture->flags = flags;
    texture->frameUsed = vk.currentFrame;
    
    // Load image data
    int width, height, channels;
    byte* imageData = R_LoadImageData(name, &width, &height, &channels);
    
    if (!imageData) {
        Com_Printf("^1Failed to load texture: %s\n", name);
        numTextures--;
        return NULL;
    }
    
    // Determine format and mip levels
    VkFormat format = (channels == 4) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8_UNORM;
    int mipLevels = (flags & TEXTURE_FLAG_NOMIPMAP) ? 1 : VK_CalculateMipLevels(width, height);
    
    texture->width = width;
    texture->height = height;
    texture->mipLevels = mipLevels;
    texture->format = format;
    
    // Create Vulkan image
    if (!VK_CreateImage(width, height, mipLevels, format,
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT,  // For mipmap generation
                       VMA_MEMORY_USAGE_GPU_ONLY, &texture->image)) {
        Com_Printf("^1Failed to create texture image for %s\n", name);
        Z_Free(imageData);
        numTextures--;
        return NULL;
    }
    
    // Create image view
    if (!VK_CreateImageView(&texture->image, VK_IMAGE_ASPECT_COLOR_BIT)) {
        Com_Printf("^1Failed to create texture image view for %s\n", name);
        VK_DestroyImage(&texture->image);
        Z_Free(imageData);
        numTextures--;
        return NULL;
    }
    
    // Upload texture data
    if (!VK_UploadTextureData(texture, imageData, width * height * channels)) {
        Com_Printf("^1Failed to upload texture data for %s\n", name);
        VK_DestroyImage(&texture->image);
        Z_Free(imageData);
        numTextures--;
        return NULL;
    }
    
    // Generate mipmaps if needed
    if (mipLevels > 1) {
        VK_GenerateMipmaps(&texture->image);
    }
    
    // Create sampler
    if (!VK_CreateTextureSampler(texture)) {
        Com_Printf("^1Failed to create texture sampler for %s\n", name);
        VK_DestroyImage(&texture->image);
        Z_Free(imageData);
        numTextures--;
        return NULL;
    }
    
    Z_Free(imageData);
    
    Com_DPrintf("Loaded texture: %s (%dx%d, %d mips)\n", name, width, height, mipLevels);
    return texture;
}

qboolean VK_UploadTextureData(texture_t* texture, const void* data, VkDeviceSize dataSize) {
    // Get staging buffer
    stagingBuffer_t* staging = VK_GetStagingBuffer(dataSize);
    if (!staging) {
        return qfalse;
    }
    
    // Copy data to staging buffer
    void* mapped;
    if (!VK_MapBuffer(&staging->buffer, &mapped)) {
        VK_ReleaseStagingBuffer(staging);
        return qfalse;
    }
    
    memcpy((byte*)mapped + staging->offset, data, dataSize);
    staging->offset += dataSize;
    
    // Record command buffer for transfer
    VkCommandBuffer commandBuffer = VK_BeginSingleTimeCommands();
    
    // Transition image to transfer destination
    VK_TransitionImageLayout(commandBuffer, &texture->image, 
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    // Copy buffer to image
    VK_CopyBufferToImage(commandBuffer, &staging->buffer, &texture->image);
    
    // Transition to shader read-only (unless we need to generate mipmaps)
    if (texture->mipLevels == 1) {
        VK_TransitionImageLayout(commandBuffer, &texture->image,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    
    VK_EndSingleTimeCommands(commandBuffer);
    
    VK_ReleaseStagingBuffer(staging);
    return qtrue;
}

void VK_GenerateMipmaps(vkImage_t* image) {
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(vk.physicalDevice, image->format, &formatProperties);
    
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        Com_Printf("^3Warning: Texture format does not support linear blitting\n");
        return;
    }
    
    VkCommandBuffer commandBuffer = VK_BeginSingleTimeCommands();
    
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .image = image->image,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1,
        .subresourceRange.levelCount = 1,
    };
    
    int32_t mipWidth = image->extent.width;
    int32_t mipHeight = image->extent.height;
    
    for (uint32_t i = 1; i < image->mipLevels; i++) {
        // Transition previous mip level to transfer source
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        
        vkCmdPipelineBarrier(commandBuffer,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                           0, NULL, 0, NULL, 1, &barrier);
        
        // Blit from previous mip level to current
        VkImageBlit blit = {
            .srcOffsets[0] = {0, 0, 0},
            .srcOffsets[1] = {mipWidth, mipHeight, 1},
            .srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .srcSubresource.mipLevel = i - 1,
            .srcSubresource.baseArrayLayer = 0,
            .srcSubresource.layerCount = 1,
            .dstOffsets[0] = {0, 0, 0},
            .dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, 
                              mipHeight > 1 ? mipHeight / 2 : 1, 1},
            .dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .dstSubresource.mipLevel = i,
            .dstSubresource.baseArrayLayer = 0,
            .dstSubresource.layerCount = 1,
        };
        
        vkCmdBlitImage(commandBuffer,
                      image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      1, &blit, VK_FILTER_LINEAR);
        
        // Transition to shader read-only
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(commandBuffer,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                           0, NULL, 0, NULL, 1, &barrier);
        
        // Update dimensions for next iteration
        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }
    
    // Transition final mip level
    barrier.subresourceRange.baseMipLevel = image->mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(commandBuffer,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                       0, NULL, 0, NULL, 1, &barrier);
    
    VK_EndSingleTimeCommands(commandBuffer);
    
    image->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Resource Garbage Collection</h2>
    
    <h3>Automatic Resource Cleanup</h3>
    <div class="code-block">
        <pre><code>// Resource garbage collection for unused resources
typedef struct resourceGC_s {
    // Tracking lists
    vkBuffer_t* buffersToDelete[MAX_FRAMES_IN_FLIGHT][MAX_RESOURCES_PER_FRAME];
    int bufferDeleteCount[MAX_FRAMES_IN_FLIGHT];
    
    vkImage_t* imagesToDelete[MAX_FRAMES_IN_FLIGHT][MAX_RESOURCES_PER_FRAME];
    int imageDeleteCount[MAX_FRAMES_IN_FLIGHT];
    
    // Statistics
    int buffersDeleted;
    int imagesDeleted;
    size_t memoryFreed;
    
} resourceGC_t;

static resourceGC_t resourceGC;

void ResourceGC_Init(void) {
    memset(&resourceGC, 0, sizeof(resourceGC));
}

void ResourceGC_MarkBufferForDeletion(vkBuffer_t* buffer) {
    int frameIndex = vk.currentFrame;
    int count = resourceGC.bufferDeleteCount[frameIndex];
    
    if (count < MAX_RESOURCES_PER_FRAME) {
        resourceGC.buffersToDelete[frameIndex][count] = buffer;
        resourceGC.bufferDeleteCount[frameIndex]++;
    } else {
        Com_Printf("^3Warning: Resource GC buffer list full, deleting immediately\n");
        VK_DestroyBuffer(buffer);
    }
}

void ResourceGC_MarkImageForDeletion(vkImage_t* image) {
    int frameIndex = vk.currentFrame;
    int count = resourceGC.imageDeleteCount[frameIndex];
    
    if (count < MAX_RESOURCES_PER_FRAME) {
        resourceGC.imagesToDelete[frameIndex][count] = image;
        resourceGC.imageDeleteCount[frameIndex]++;
    } else {
        Com_Printf("^3Warning: Resource GC image list full, deleting immediately\n");
        VK_DestroyImage(image);
    }
}

void ResourceGC_CollectFrame(int frameIndex) {
    // Delete buffers that are now safe to delete
    for (int i = 0; i < resourceGC.bufferDeleteCount[frameIndex]; i++) {
        vkBuffer_t* buffer = resourceGC.buffersToDelete[frameIndex][i];
        if (buffer) {
            resourceGC.memoryFreed += buffer->size;
            VK_DestroyBuffer(buffer);
            resourceGC.buffersDeleted++;
        }
    }
    resourceGC.bufferDeleteCount[frameIndex] = 0;
    
    // Delete images that are now safe to delete
    for (int i = 0; i < resourceGC.imageDeleteCount[frameIndex]; i++) {
        vkImage_t* image = resourceGC.imagesToDelete[frameIndex][i];
        if (image) {
            resourceGC.memoryFreed += image->allocInfo.size;
            VK_DestroyImage(image);
            resourceGC.imagesDeleted++;
        }
    }
    resourceGC.imageDeleteCount[frameIndex] = 0;
}

void ResourceGC_RunCollection(void) {
    // Collect resources from frames that are guaranteed to be finished
    int frameToCollect = (vk.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    ResourceGC_CollectFrame(frameToCollect);
}

// Periodic cleanup of unused textures
void ResourceGC_CleanupUnusedTextures(void) {
    int currentFrame = vk.currentFrame;
    int texturesRemoved = 0;
    
    for (int i = numTextures - 1; i >= 0; i--) {
        texture_t* texture = &textures[i];
        
        // Skip recently used textures
        if (currentFrame - texture->frameUsed < 300) {  // 5 seconds at 60 FPS
            continue;
        }
        
        // Skip important textures
        if (texture->flags & TEXTURE_FLAG_NOPICMIP) {
            continue;
        }
        
        Com_DPrintf("Removing unused texture: %s\n", texture->name);
        
        // Cleanup texture resources
        if (texture->sampler) {
            vkDestroySampler(vk.device, texture->sampler, NULL);
        }
        
        ResourceGC_MarkImageForDeletion(&texture->image);
        
        // Remove from array
        if (i < numTextures - 1) {
            memmove(&textures[i], &textures[i + 1], 
                   (numTextures - i - 1) * sizeof(texture_t));
        }
        numTextures--;
        texturesRemoved++;
    }
    
    if (texturesRemoved > 0) {
        Com_Printf("Resource GC: Removed %d unused textures\n", texturesRemoved);
    }
}

void ResourceGC_PrintStats(void) {
    Com_Printf("Resource GC Statistics:\n");
    Com_Printf("  Buffers deleted: %d\n", resourceGC.buffersDeleted);
    Com_Printf("  Images deleted: %d\n", resourceGC.imagesDeleted);
    Com_Printf("  Memory freed: %zu MB\n", resourceGC.memoryFreed / (1024 * 1024));
    Com_Printf("  Active textures: %d\n", numTextures);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Memory Budget Management</h2>
    
    <h3>Dynamic Quality Adjustment</h3>
    <div class="code-block">
        <pre><code>// Dynamic quality adjustment based on memory pressure
typedef struct memoryBudget_s {
    size_t totalBudget;         // Total VRAM budget
    size_t currentUsage;        // Current usage
    size_t warningThreshold;    // 75% of budget
    size_t criticalThreshold;   // 90% of budget
    
    int qualityLevel;           // 0-3 (low to ultra)
    qboolean memoryPressure;    // Under memory pressure
    
} memoryBudget_t;

static memoryBudget_t memBudget;

void Memory_InitBudget(void) {
    // Get total device memory
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(vk.physicalDevice, &memProps);
    
    // Find largest device local heap
    size_t maxHeapSize = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            if (memProps.memoryHeaps[i].size > maxHeapSize) {
                maxHeapSize = memProps.memoryHeaps[i].size;
            }
        }
    }
    
    // Set budget to 80% of largest heap
    memBudget.totalBudget = (size_t)(maxHeapSize * 0.8f);
    memBudget.warningThreshold = (size_t)(memBudget.totalBudget * 0.75f);
    memBudget.criticalThreshold = (size_t)(memBudget.totalBudget * 0.90f);
    memBudget.qualityLevel = 2;  // Start at high quality
    
    Com_Printf("Memory budget: %zu MB\n", memBudget.totalBudget / (1024 * 1024));
}

void Memory_UpdateBudget(void) {
    // Get current memory usage
    vmaGetBudget(vma.allocator, vma.budgets);
    
    memBudget.currentUsage = 0;
    for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; i++) {
        if (vma.budgets[i].budget > 0) {
            memBudget.currentUsage += vma.budgets[i].usage;
        }
    }
    
    // Check for memory pressure
    qboolean oldPressure = memBudget.memoryPressure;
    
    if (memBudget.currentUsage > memBudget.criticalThreshold) {
        memBudget.memoryPressure = qtrue;
        
        if (!oldPressure) {
            Com_Printf("^1Critical memory pressure detected\n");
            Memory_ReduceQuality();
        }
    } else if (memBudget.currentUsage > memBudget.warningThreshold) {
        if (!memBudget.memoryPressure) {
            Com_Printf("^3Memory pressure warning\n");
            Memory_ReduceQuality();
        }
        memBudget.memoryPressure = qtrue;
    } else {
        memBudget.memoryPressure = qfalse;
        
        if (oldPressure) {
            Com_Printf("Memory pressure relieved\n");
        }
    }
}

void Memory_ReduceQuality(void) {
    if (memBudget.qualityLevel <= 0) {
        return; // Already at minimum quality
    }
    
    memBudget.qualityLevel--;
    
    switch (memBudget.qualityLevel) {
    case 2: // High -> Medium
        Com_Printf("Reducing to medium quality\n");
        Cvar_Set("r_picmip", "1");
        Cvar_Set("r_texturebits", "16");
        break;
        
    case 1: // Medium -> Low
        Com_Printf("Reducing to low quality\n");
        Cvar_Set("r_picmip", "2");
        Cvar_Set("r_shadowMapSize", "512");
        break;
        
    case 0: // Low -> Minimum
        Com_Printf("Reducing to minimum quality\n");
        Cvar_Set("r_picmip", "3");
        Cvar_Set("r_shadows", "0");
        Cvar_Set("r_bloom", "0");
        break;
    }
    
    // Force texture reload with new settings
    ResourceGC_CleanupUnusedTextures();
}

void Memory_RestoreQuality(void) {
    if (memBudget.memoryPressure || memBudget.qualityLevel >= 3) {
        return;
    }
    
    // Gradually restore quality when memory pressure is low
    static int lastRestore = 0;
    int currentTime = Sys_Milliseconds();
    
    if (currentTime - lastRestore < 10000) {  // Wait 10 seconds between restorations
        return;
    }
    
    memBudget.qualityLevel++;
    lastRestore = currentTime;
    
    switch (memBudget.qualityLevel) {
    case 1: // Minimum -> Low
        Com_Printf("Restoring to low quality\n");
        Cvar_Set("r_picmip", "2");
        Cvar_Set("r_shadows", "1");
        break;
        
    case 2: // Low -> Medium
        Com_Printf("Restoring to medium quality\n");
        Cvar_Set("r_picmip", "1");
        Cvar_Set("r_shadowMapSize", "1024");
        break;
        
    case 3: // Medium -> High
        Com_Printf("Restoring to high quality\n");
        Cvar_Set("r_picmip", "0");
        Cvar_Set("r_texturebits", "32");
        Cvar_Set("r_bloom", "1");
        break;
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/renderer/vulkan-implementation">Vulkan Renderer</a></li>
        <li><a href="/renderer/pbr-pipeline">PBR Pipeline</a></li>
        <li><a href="/core/memory-management">Core Memory Management</a></li>
        <li><a href="/platform/cross-platform">Cross-Platform Development</a></li>
        <li><a href="/modernization/profiling-tools">Performance Profiling</a></li>
    </ul>
</div>