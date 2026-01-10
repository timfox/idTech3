#include "tr_local.h"
#include "vk_images.h"
#include "vk_utils.h"
#include "vk_memory.h"
#include "vk_pbo.h"
#include "vk.h"
#include <math.h>

// Renderer interface
extern refimport_t ri;

// Vulkan function pointer extern declarations
extern PFN_vkCreateImage qvkCreateImage;
extern PFN_vkDestroyImage qvkDestroyImage;
extern PFN_vkGetImageMemoryRequirements qvkGetImageMemoryRequirements;
extern PFN_vkCreateImageView qvkCreateImageView;
extern PFN_vkDestroyImageView qvkDestroyImageView;
extern PFN_vkBindImageMemory qvkBindImageMemory;
extern PFN_vkCmdCopyBufferToImage qvkCmdCopyBufferToImage;
extern PFN_vkCmdPipelineBarrier qvkCmdPipelineBarrier;

// Utility functions
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

// Memory tracking functions
extern void vk_track_allocation(VkDeviceSize size);
extern void vk_track_free(VkDeviceSize size);

// Forward declarations
static void record_image_layout_transition(VkCommandBuffer command_buffer, VkImage image,
    VkImageAspectFlags aspect_mask, VkImageLayout old_layout, VkImageLayout new_layout,
    VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask);

// Create Vulkan image
extern "C" void vk_create_image(image_t *image, int width, int height, int mip_levels) {
    VkFormat format = (VkFormat)image->internalFormat;
    VkImageCreateInfo desc;
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo alloc_info;
    uint32_t memory_type;
    VkDeviceMemory memory;

    if (!vk_validate_handle(vk.device, "device")) {
        return;
    }

    desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.imageType = VK_IMAGE_TYPE_2D;
    desc.format = format;
    desc.extent.width = width;
    desc.extent.height = height;
    desc.extent.depth = 1;
    desc.mipLevels = mip_levels;
    desc.arrayLayers = 1;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.tiling = VK_IMAGE_TILING_OPTIMAL;
    desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = NULL;
    desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK_CHECK(qvkCreateImage(vk.device, &desc, NULL, &image->handle));

    qvkGetImageMemoryRequirements(vk.device, image->handle, &memory_requirements);

    memory_type = find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = memory_type;

    VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory));
    vk_track_allocation(memory_requirements.size);

    // Track GPU memory allocation for leak detection
    vk_track_gpu_allocation(memory, memory_requirements.size, memory_type,
                           image->imgName, "vk_images.cpp:create_color_attachment");

    // Record memory access for bandwidth profiling
    vk_record_memory_access((void*)memory, memory_requirements.size, image->imgName, qtrue);

    VK_CHECK(qvkBindImageMemory(vk.device, image->handle, memory, 0));

    image->memory = memory;

    SET_OBJECT_NAME(image->handle, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
    SET_OBJECT_NAME(image->memory, va("%s memory", image->imgName), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);
}

// Create image view
extern "C" void vk_create_image_view(image_t *image, VkImageViewType view_type, VkImageAspectFlags aspect) {
    VkImageViewCreateInfo desc;

    if (!vk_validate_handle(image->handle, "image")) {
        return;
    }

    desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.image = image->handle;
    desc.viewType = view_type;
    desc.format = (VkFormat)image->internalFormat;
    desc.components.r = VK_COMPONENT_SWIZZLE_R;
    desc.components.g = VK_COMPONENT_SWIZZLE_G;
    desc.components.b = VK_COMPONENT_SWIZZLE_B;
    desc.components.a = VK_COMPONENT_SWIZZLE_A;
    desc.subresourceRange.aspectMask = aspect;
    desc.subresourceRange.baseMipLevel = 0;
    desc.subresourceRange.levelCount = (uint32_t)(floor(log2(image->width))) + 1;
    desc.subresourceRange.baseArrayLayer = 0;
    desc.subresourceRange.layerCount = 1;

    VK_CHECK(qvkCreateImageView(vk.device, &desc, NULL, &image->view));

    SET_OBJECT_NAME(image->view, va("%s view", image->imgName), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);
}

// Destroy image and associated resources
extern "C" void vk_destroy_image(image_t *image) {
    // Track memory deallocation FIRST, even if device is lost
    // This is critical - we need to update our internal tracking so recovery knows memory is available
    if (image->memory != VK_NULL_HANDLE) {
        // Always update tracking, even if device is lost
        // The driver may have already freed the memory, but we need to update our bookkeeping
        vk_track_gpu_free(image->memory);
    }

    // Skip all Vulkan API calls if device is lost - driver may have already destroyed resources
    if (vk.device_lost || vk.device == VK_NULL_HANDLE) {
        // Just clear the handles to prevent use-after-free, but don't call Vulkan API
        // Memory tracking has already been updated above
        image->memory = VK_NULL_HANDLE;
        image->view = VK_NULL_HANDLE;
        image->handle = VK_NULL_HANDLE;
        return;
    }

    if (image->memory != VK_NULL_HANDLE) {
        // Free the memory (tracking already done above)
        qvkFreeMemory(vk.device, image->memory, NULL);
        image->memory = VK_NULL_HANDLE;
    }

    if (image->view != VK_NULL_HANDLE) {
        qvkDestroyImageView(vk.device, image->view, NULL);
        image->view = VK_NULL_HANDLE;
    }

    if (image->handle != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, image->handle, NULL);
        image->handle = VK_NULL_HANDLE;
    }
}

// Upload image data to GPU
extern "C" void vk_upload_image_data(image_t *image, int x, int y, int width, int height, int layers, const void *data, int data_size, qboolean update) {
    VkBufferImageCopy regions[16];
    int num_regions = 0;
    int buffer_size = data_size;
    VkCommandBuffer command_buffer;
    int w = width;
    int h = height;

    if (!vk_validate_handle(image->handle, "image")) {
        return;
    }

    if (data == NULL && data_size > 0) {
        ri.Printf(PRINT_ERROR, "vk_upload_image_data: data is NULL but size > 0\n");
        return;
    }

    // Try PBO system first if available and enabled
    if (vk_pbo_is_available() && r_pbo->integer) {
        VkBufferImageCopy region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .imageOffset = {x, y, 0},
            .imageExtent = {(uint32_t)width, (uint32_t)height, 1}
        };

        VkImageLayout final_layout = update ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        if (vk_pbo_upload_texture_sync(data, data_size, image->handle, final_layout, &region)) {
            return; // PBO upload succeeded
        }
        // Fall back to traditional upload if PBO fails
    }

    // Calculate number of mip levels
    int mipmaps = layers;
    if (mipmaps == 0) {
        mipmaps = 1;
    }

    // Prepare copy regions for all mip levels
    for (int i = 0; i < mipmaps; i++) {
        VkBufferImageCopy *region = &regions[num_regions];

        region->bufferOffset = vk.staging_buffer.offset;
        region->bufferRowLength = 0;
        region->bufferImageHeight = 0;
        region->imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region->imageSubresource.mipLevel = i;
        region->imageSubresource.baseArrayLayer = 0;
        region->imageSubresource.layerCount = 1;
        region->imageOffset.x = x >> i;
        region->imageOffset.y = y >> i;
        region->imageOffset.z = 0;
        region->imageExtent.width = MAX(1, w >> i);
        region->imageExtent.height = MAX(1, h >> i);
        region->imageExtent.depth = 1;

        num_regions++;

        if (static_cast<size_t>(num_regions) >= static_cast<size_t>(mipmaps) || (width == 1 && height == 1) || static_cast<size_t>(num_regions) >= ARRAY_LEN(regions))
            break;

        x >>= 1;
        y >>= 1;
        width >>= 1;
        if (width < 1) width = 1;
        height >>= 1;
        if (height < 1) height = 1;
    }

#ifdef USE_UPLOAD_QUEUE
    if (vk_wait_staging_buffer()) {
        // wait for vkQueueSubmit() completion before new upload
    }

    if (vk.staging_buffer.size - vk.staging_buffer.offset < static_cast<VkDeviceSize>(buffer_size)) {
        // try to flush staging buffer and reset offset
        vk_flush_staging_buffer(qfalse);
    }

    if (vk.staging_buffer.size /* - vk_world.staging_buffer_offset */ < static_cast<VkDeviceSize>(buffer_size)) {
        // if still not enough - reallocate staging buffer
        vk_alloc_staging_buffer(buffer_size);
    }

    for (int n = 0; n < num_regions; n++) {
        regions[n].bufferOffset += vk.staging_buffer.offset;
    }

    // Bounds check before memcpy
    if (vk.staging_buffer.offset + buffer_size > vk.staging_buffer.size) {
        ri.Printf(PRINT_ERROR, "vk_upload_image_data: staging buffer overflow! offset=%lu size=%lu buffer_size=%lu\n",
            (unsigned long)vk.staging_buffer.offset, (unsigned long)vk.staging_buffer.size, (unsigned long)buffer_size);
        return;
    }
    Com_Memcpy(vk.staging_buffer.ptr + vk.staging_buffer.offset, data, buffer_size);

    if (vk.staging_buffer.offset == 0) {
        VkCommandBufferBeginInfo begin_info;
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.pNext = NULL;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        begin_info.pInheritanceInfo = NULL;
        VK_CHECK(qvkBeginCommandBuffer(vk.staging_command_buffer, &begin_info));
    }

    //ri.Printf( PRINT_WARNING, "batch @%6i + %i %s \n", (int)vk_world.staging_buffer_offset, (int)buffer_size, image->imgName );
    vk.staging_buffer.offset += buffer_size;

    command_buffer = vk.staging_command_buffer;

    if (update) {
        record_image_layout_transition(command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0);
    } else {
        record_image_layout_transition(command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0);
    }

    qvkCmdCopyBufferToImage(command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions);

    // final transition after upload completed
    record_image_layout_transition(command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0);
#else
    if (vk.staging_buffer.size < buffer_size) {
        vk_alloc_staging_buffer(buffer_size);
    }

    // Bounds check before memcpy
    if (buffer_size > vk.staging_buffer.size) {
        ri.Printf(PRINT_ERROR, "vk_upload_image_data: buffer_size %lu exceeds staging buffer size %lu\n",
            (unsigned long)buffer_size, (unsigned long)vk.staging_buffer.size);
        return;
    }
    Com_Memcpy(vk.staging_buffer.ptr, data, buffer_size);

    command_buffer = begin_command_buffer();
    // record_buffer_memory_barrier( command_buffer, vk_world.staging_buffer, VK_WHOLE_SIZE, 0, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );
    if (update) {
        record_image_layout_transition(command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0);
    } else {
        record_image_layout_transition(command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0);
    }
    qvkCmdCopyBufferToImage(command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions);
    record_image_layout_transition(command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0);
    end_command_buffer(command_buffer, __func__);
#endif
}

// Helper function for image layout transitions
static void record_image_layout_transition(VkCommandBuffer command_buffer, VkImage image,
    VkImageAspectFlags aspect_mask, VkImageLayout old_layout, VkImageLayout new_layout,
    VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask) {

    VkImageMemoryBarrier barrier;

    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = NULL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect_mask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    // Determine access masks based on layouts
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }

    if (src_stage_mask == 0) {
        src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
    if (dst_stage_mask == 0) {
        dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    qvkCmdPipelineBarrier(command_buffer, src_stage_mask, dst_stage_mask, 0, 0, NULL, 0, NULL, 1, &barrier);
}
