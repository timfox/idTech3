#pragma once

#include "tr_local.h"

// Dynamic Rendering System
// Uses VK_KHR_dynamic_rendering for flexible rendering without traditional render passes
// Supports multisampling, multiple color attachments, and deferred rendering

// Dynamic rendering attachment info
typedef struct {
    VkImageView imageView;
    VkImageLayout imageLayout;
    VkAttachmentLoadOp loadOp;
    VkAttachmentStoreOp storeOp;
    VkClearValue clearValue;
    VkResolveModeFlagBits resolveMode;
    VkImageView resolveImageView;
    VkImageLayout resolveImageLayout;
} vk_dynamic_attachment_t;

// Dynamic rendering state
typedef struct {
    qboolean enabled;
    qboolean initialized;
    qboolean extensionAvailable;

    // Function pointers for dynamic rendering commands
    PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
    PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR;

    // Multisampling support
    VkSampleCountFlagBits msaaSamples;
    qboolean msaaEnabled;

    // Intermediate render images for multisampling
    struct {
        VkImage colorImage;
        VkImageView colorView;
        VkDeviceMemory colorMemory;
        VkImage depthImage;
        VkImageView depthView;
        VkDeviceMemory depthMemory;
    } msaaImages;

    // Settings
    qboolean useDynamicRendering;
    int maxColorAttachments;
} vk_dynamic_rendering_t;

extern vk_dynamic_rendering_t vk_dynamic_rendering;

// Dynamic Rendering API
void VK_DynamicRendering_Init(void);
void VK_DynamicRendering_Shutdown(void);
qboolean VK_DynamicRendering_IsSupported(void);
void VK_DynamicRendering_Begin(VkCommandBuffer commandBuffer, 
                                const VkRect2D* renderArea,
                                int colorAttachmentCount,
                                const vk_dynamic_attachment_t* colorAttachments,
                                const vk_dynamic_attachment_t* depthAttachment,
                                const vk_dynamic_attachment_t* stencilAttachment);
void VK_DynamicRendering_End(VkCommandBuffer commandBuffer);
void VK_DynamicRendering_CreateMSAAImages(int width, int height, VkSampleCountFlagBits samples);
void VK_DynamicRendering_DestroyMSAAImages(void);
VkImageView VK_DynamicRendering_GetMSAAColorView(void);
VkImageView VK_DynamicRendering_GetMSAADepthView(void);
void VK_DynamicRendering_SetMSAAEnabled(qboolean enabled);
VkSampleCountFlagBits VK_DynamicRendering_GetMSAASamples(void);

// Pipeline creation helpers for dynamic rendering
void VK_DynamicRendering_SetupPipelineRenderingInfo(
    VkPipelineRenderingCreateInfoKHR* renderingInfo,
    int colorAttachmentCount,
    const VkFormat* colorFormats,
    VkFormat depthFormat,
    VkFormat stencilFormat);
