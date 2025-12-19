#include "vk_dynamic_rendering.h"
#include "vk.h"
#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "../../qcommon/qcommon.h"
#include <string.h>

#ifndef VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
#define VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME "VK_KHR_dynamic_rendering"
#endif

vk_dynamic_rendering_t vk_dynamic_rendering;

void VK_DynamicRendering_Init(void) {
    memset(&vk_dynamic_rendering, 0, sizeof(vk_dynamic_rendering_t));

    // Check if dynamic rendering extension is available
    uint32_t extensionCount = 0;
    qvkEnumerateDeviceExtensionProperties(vk.physical_device, NULL, &extensionCount, NULL);
    
    if (extensionCount > 0) {
        VkExtensionProperties* extensions = (VkExtensionProperties*)ri.Hunk_AllocateTempMemory(
            extensionCount * sizeof(VkExtensionProperties));
        qvkEnumerateDeviceExtensionProperties(vk.physical_device, NULL, &extensionCount, extensions);

        for (uint32_t i = 0; i < extensionCount; i++) {
            // Check for dynamic rendering extension
            if (strcmp(extensions[i].extensionName, "VK_KHR_dynamic_rendering") == 0 ||
                strcmp(extensions[i].extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0) {
                vk_dynamic_rendering.extensionAvailable = qtrue;
                break;
            }
        }

        ri.Hunk_FreeTempMemory(extensions);
    }

    if (!vk_dynamic_rendering.extensionAvailable) {
        ri.Printf(PRINT_WARNING, "VK_KHR_dynamic_rendering extension not available\n");
        vk_dynamic_rendering.enabled = qfalse;
        vk_dynamic_rendering.initialized = qtrue;
        return;
    }

    // Load function pointers
    vk_dynamic_rendering.vkCmdBeginRenderingKHR = (PFN_vkCmdBeginRenderingKHR)
        qvkGetDeviceProcAddr(vk.device, "vkCmdBeginRenderingKHR");
    vk_dynamic_rendering.vkCmdEndRenderingKHR = (PFN_vkCmdEndRenderingKHR)
        qvkGetDeviceProcAddr(vk.device, "vkCmdEndRenderingKHR");

    if (!vk_dynamic_rendering.vkCmdBeginRenderingKHR || !vk_dynamic_rendering.vkCmdEndRenderingKHR) {
        ri.Printf(PRINT_WARNING, "Failed to load dynamic rendering function pointers\n");
        vk_dynamic_rendering.extensionAvailable = qfalse;
        vk_dynamic_rendering.enabled = qfalse;
        vk_dynamic_rendering.initialized = qtrue;
        return;
    }

    // Default settings
    vk_dynamic_rendering.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    vk_dynamic_rendering.msaaEnabled = qfalse;
    vk_dynamic_rendering.useDynamicRendering = qfalse;  // Off by default, can be enabled via CVar
    vk_dynamic_rendering.maxColorAttachments = 8;  // Typical limit
    vk_dynamic_rendering.enabled = qtrue;
    vk_dynamic_rendering.initialized = qtrue;

    ri.Printf(PRINT_ALL, "Dynamic rendering system initialized (VK_KHR_dynamic_rendering)\n");
}

void VK_DynamicRendering_Shutdown(void) {
    if (!vk_dynamic_rendering.initialized) return;

    VK_DynamicRendering_DestroyMSAAImages();

    memset(&vk_dynamic_rendering, 0, sizeof(vk_dynamic_rendering_t));
}

qboolean VK_DynamicRendering_IsSupported(void) {
    return vk_dynamic_rendering.extensionAvailable && vk_dynamic_rendering.initialized;
}

void VK_DynamicRendering_Begin(VkCommandBuffer commandBuffer,
                                const VkRect2D* renderArea,
                                int colorAttachmentCount,
                                const vk_dynamic_attachment_t* colorAttachments,
                                const vk_dynamic_attachment_t* depthAttachment,
                                const vk_dynamic_attachment_t* stencilAttachment) {
    if (!vk_dynamic_rendering.enabled || !vk_dynamic_rendering.extensionAvailable) return;
    if (!vk_dynamic_rendering.vkCmdBeginRenderingKHR) return;

    VkRenderingAttachmentInfoKHR* colorInfos = NULL;
    if (colorAttachmentCount > 0 && colorAttachments) {
        colorInfos = (VkRenderingAttachmentInfoKHR*)ri.Hunk_AllocateTempMemory(
            colorAttachmentCount * sizeof(VkRenderingAttachmentInfoKHR));

        for (int i = 0; i < colorAttachmentCount; i++) {
            colorInfos[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            colorInfos[i].imageView = colorAttachments[i].imageView;
            colorInfos[i].imageLayout = colorAttachments[i].imageLayout;
            colorInfos[i].loadOp = colorAttachments[i].loadOp;
            colorInfos[i].storeOp = colorAttachments[i].storeOp;
            colorInfos[i].clearValue = colorAttachments[i].clearValue;
            colorInfos[i].resolveMode = colorAttachments[i].resolveMode;
            colorInfos[i].resolveImageView = colorAttachments[i].resolveImageView;
            colorInfos[i].resolveImageLayout = colorAttachments[i].resolveImageLayout;
        }
    }

    VkRenderingAttachmentInfoKHR* depthInfo = NULL;
    if (depthAttachment) {
        depthInfo = (VkRenderingAttachmentInfoKHR*)ri.Hunk_AllocateTempMemory(
            sizeof(VkRenderingAttachmentInfoKHR));
        depthInfo->sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        depthInfo->imageView = depthAttachment->imageView;
        depthInfo->imageLayout = depthAttachment->imageLayout;
        depthInfo->loadOp = depthAttachment->loadOp;
        depthInfo->storeOp = depthAttachment->storeOp;
        depthInfo->clearValue = depthAttachment->clearValue;
    }

    VkRenderingAttachmentInfoKHR* stencilInfo = NULL;
    if (stencilAttachment && stencilAttachment->imageView != VK_NULL_HANDLE) {
        stencilInfo = (VkRenderingAttachmentInfoKHR*)ri.Hunk_AllocateTempMemory(
            sizeof(VkRenderingAttachmentInfoKHR));
        stencilInfo->sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        stencilInfo->imageView = stencilAttachment->imageView;
        stencilInfo->imageLayout = stencilAttachment->imageLayout;
        stencilInfo->loadOp = stencilAttachment->loadOp;
        stencilInfo->storeOp = stencilAttachment->storeOp;
        stencilInfo->clearValue = stencilAttachment->clearValue;
    } else if (depthAttachment) {
        // Use depth attachment for stencil if not specified separately
        stencilInfo = depthInfo;
    }

    VkRenderingInfoKHR renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.renderArea = *renderArea;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorAttachmentCount;
    renderingInfo.pColorAttachments = colorInfos;
    renderingInfo.pDepthAttachment = depthInfo;
    renderingInfo.pStencilAttachment = stencilInfo;

    vk_dynamic_rendering.vkCmdBeginRenderingKHR(commandBuffer, &renderingInfo);

    // Free temporary memory (will be freed at end of frame)
    // Note: In a production system, you'd want to manage this memory more carefully
}

void VK_DynamicRendering_End(VkCommandBuffer commandBuffer) {
    if (!vk_dynamic_rendering.enabled || !vk_dynamic_rendering.extensionAvailable) return;
    if (!vk_dynamic_rendering.vkCmdEndRenderingKHR) return;

    vk_dynamic_rendering.vkCmdEndRenderingKHR(commandBuffer);
}

void VK_DynamicRendering_CreateMSAAImages(int width, int height, VkSampleCountFlagBits samples) {
    if (!vk_dynamic_rendering.initialized) return;

    // Destroy existing images if they exist
    VK_DynamicRendering_DestroyMSAAImages();

    if (samples == VK_SAMPLE_COUNT_1_BIT) {
        vk_dynamic_rendering.msaaEnabled = qfalse;
        return;
    }

    vk_dynamic_rendering.msaaSamples = samples;
    vk_dynamic_rendering.msaaEnabled = qtrue;

    // Create color MSAA image
    VkImageCreateInfo colorImageInfo = {};
    colorImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    colorImageInfo.imageType = VK_IMAGE_TYPE_2D;
    colorImageInfo.format = vk.swapchain_format;
    colorImageInfo.extent.width = width;
    colorImageInfo.extent.height = height;
    colorImageInfo.extent.depth = 1;
    colorImageInfo.mipLevels = 1;
    colorImageInfo.arrayLayers = 1;
    colorImageInfo.samples = samples;
    colorImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    colorImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK_CHECK(qvkCreateImage(vk.device, &colorImageInfo, NULL, &vk_dynamic_rendering.msaaImages.colorImage));

    VkMemoryRequirements memReq;
    qvkGetImageMemoryRequirements(vk.device, vk_dynamic_rendering.msaaImages.colorImage, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk_dynamic_rendering.msaaImages.colorMemory));
    VK_CHECK(qvkBindImageMemory(vk.device, vk_dynamic_rendering.msaaImages.colorImage,
        vk_dynamic_rendering.msaaImages.colorMemory, 0));

    // Create color image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vk_dynamic_rendering.msaaImages.colorImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vk.swapchain_format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(qvkCreateImageView(vk.device, &viewInfo, NULL, &vk_dynamic_rendering.msaaImages.colorView));

    // Create depth MSAA image
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;  // Would query supported depth format in full implementation

    VkImageCreateInfo depthImageInfo = {};
    depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageInfo.format = depthFormat;
    depthImageInfo.extent.width = width;
    depthImageInfo.extent.height = height;
    depthImageInfo.extent.depth = 1;
    depthImageInfo.mipLevels = 1;
    depthImageInfo.arrayLayers = 1;
    depthImageInfo.samples = samples;
    depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK_CHECK(qvkCreateImage(vk.device, &depthImageInfo, NULL, &vk_dynamic_rendering.msaaImages.depthImage));

    qvkGetImageMemoryRequirements(vk.device, vk_dynamic_rendering.msaaImages.depthImage, &memReq);

    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk_dynamic_rendering.msaaImages.depthMemory));
    VK_CHECK(qvkBindImageMemory(vk.device, vk_dynamic_rendering.msaaImages.depthImage,
        vk_dynamic_rendering.msaaImages.depthMemory, 0));

    // Create depth image view
    viewInfo.image = vk_dynamic_rendering.msaaImages.depthImage;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    VK_CHECK(qvkCreateImageView(vk.device, &viewInfo, NULL, &vk_dynamic_rendering.msaaImages.depthView));

    ri.Printf(PRINT_ALL, "Created MSAA images: %dx%d, %d samples\n", width, height, samples);
}

void VK_DynamicRendering_DestroyMSAAImages(void) {
    if (vk_dynamic_rendering.msaaImages.colorView != VK_NULL_HANDLE) {
        qvkDestroyImageView(vk.device, vk_dynamic_rendering.msaaImages.colorView, NULL);
        vk_dynamic_rendering.msaaImages.colorView = VK_NULL_HANDLE;
    }
    if (vk_dynamic_rendering.msaaImages.colorImage != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk_dynamic_rendering.msaaImages.colorImage, NULL);
        vk_dynamic_rendering.msaaImages.colorImage = VK_NULL_HANDLE;
    }
    if (vk_dynamic_rendering.msaaImages.colorMemory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk_dynamic_rendering.msaaImages.colorMemory, NULL);
        vk_dynamic_rendering.msaaImages.colorMemory = VK_NULL_HANDLE;
    }

    if (vk_dynamic_rendering.msaaImages.depthView != VK_NULL_HANDLE) {
        qvkDestroyImageView(vk.device, vk_dynamic_rendering.msaaImages.depthView, NULL);
        vk_dynamic_rendering.msaaImages.depthView = VK_NULL_HANDLE;
    }
    if (vk_dynamic_rendering.msaaImages.depthImage != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk_dynamic_rendering.msaaImages.depthImage, NULL);
        vk_dynamic_rendering.msaaImages.depthImage = VK_NULL_HANDLE;
    }
    if (vk_dynamic_rendering.msaaImages.depthMemory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk_dynamic_rendering.msaaImages.depthMemory, NULL);
        vk_dynamic_rendering.msaaImages.depthMemory = VK_NULL_HANDLE;
    }
}

VkImageView VK_DynamicRendering_GetMSAAColorView(void) {
    return vk_dynamic_rendering.msaaImages.colorView;
}

VkImageView VK_DynamicRendering_GetMSAADepthView(void) {
    return vk_dynamic_rendering.msaaImages.depthView;
}

void VK_DynamicRendering_SetMSAAEnabled(qboolean enabled) {
    vk_dynamic_rendering.msaaEnabled = enabled;
}

VkSampleCountFlagBits VK_DynamicRendering_GetMSAASamples(void) {
    return vk_dynamic_rendering.msaaSamples;
}

void VK_DynamicRendering_SetupPipelineRenderingInfo(
    VkPipelineRenderingCreateInfoKHR* renderingInfo,
    int colorAttachmentCount,
    const VkFormat* colorFormats,
    VkFormat depthFormat,
    VkFormat stencilFormat) {
    
    if (!renderingInfo) return;

    renderingInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    renderingInfo->pNext = NULL;
    renderingInfo->colorAttachmentCount = colorAttachmentCount;
    renderingInfo->pColorAttachmentFormats = colorFormats;
    renderingInfo->depthAttachmentFormat = depthFormat;
    renderingInfo->stencilAttachmentFormat = stencilFormat;
}
