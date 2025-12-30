/*
=============================================================================
Vulkan Framebuffer Management - C++23 Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include <vector>
#include <algorithm>

#ifdef USE_VULKAN

// External Vulkan function pointers
extern "C" {
extern PFN_vkCreateFramebuffer qvkCreateFramebuffer;
extern PFN_vkDestroyFramebuffer qvkDestroyFramebuffer;

// Global Vulkan structures
extern VkInstance vk_instance;
extern Vk_Instance vk;
}

namespace framebuffer_mgr {

// Framebuffer configuration
struct FramebufferConfig {
    VkRenderPass renderPass;
    uint32_t width;
    uint32_t height;
    uint32_t layers;
    std::vector<VkImageView> attachments;
};

// Create framebuffers for swapchain images
static VkResult create_swapchain_framebuffers(const FramebufferConfig& config, VkFramebuffer** framebuffers) {
    if (!config.renderPass || config.attachments.empty()) {
        ri.Printf(PRINT_ERROR, "Vulkan: Invalid framebuffer configuration\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t framebufferCount = config.attachments.size();
    *framebuffers = (VkFramebuffer*)ri.Malloc(framebufferCount * sizeof(VkFramebuffer));

    for (uint32_t i = 0; i < framebufferCount; i++) {
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = config.renderPass;
        framebufferInfo.attachmentCount = 1; // Only color attachment for now
        framebufferInfo.pAttachments = &config.attachments[i];
        framebufferInfo.width = config.width;
        framebufferInfo.height = config.height;
        framebufferInfo.layers = config.layers;

        VkResult result = qvkCreateFramebuffer(vk.device, &framebufferInfo, nullptr, &(*framebuffers)[i]);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "Vulkan: Failed to create framebuffer %u: %s\n", i, vk_result_string(result));
            // Clean up previously created framebuffers
            for (uint32_t j = 0; j < i; j++) {
                if ((*framebuffers)[j] != VK_NULL_HANDLE) {
                    qvkDestroyFramebuffer(vk.device, (*framebuffers)[j], nullptr);
                }
            }
            ri.Free(*framebuffers);
            *framebuffers = nullptr;
            return result;
        }
    }

    ri.Printf(PRINT_ALL, "Vulkan: Created %u framebuffers successfully\n", framebufferCount);
    return VK_SUCCESS;
}

} // namespace framebuffer_mgr

// Public interface functions
VkResult vk_create_framebuffers_enhanced(VkRenderPass renderPass, uint32_t width, uint32_t height,
                                       std::vector<VkImageView>& imageViews, VkFramebuffer** framebuffers) {
    framebuffer_mgr::FramebufferConfig config = {};
    config.renderPass = renderPass;
    config.width = width;
    config.height = height;
    config.layers = 1;
    config.attachments = imageViews;

    return framebuffer_mgr::create_swapchain_framebuffers(config, framebuffers);
}

#endif // USE_VULKAN