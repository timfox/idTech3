/*
=============================================================================
Vulkan Render Pass Management - C++23 Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include <vector>
#include <algorithm>

#ifdef USE_VULKAN

// External Vulkan function pointers
extern "C" {
extern PFN_vkCreateRenderPass qvkCreateRenderPass;
extern PFN_vkDestroyRenderPass qvkDestroyRenderPass;

// Global Vulkan structures
extern VkInstance vk_instance;
extern Vk_Instance vk;
}

namespace render_pass_mgr {

// Render pass configuration
struct RenderPassConfig {
    VkFormat colorFormat;
    VkFormat depthFormat;
    bool hasDepth;
    bool clearColor;
    bool clearDepth;
    VkSampleCountFlagBits samples;
};

// Create a basic render pass for swapchain rendering
static VkResult create_basic_render_pass(const RenderPassConfig& config, VkRenderPass* renderPass) {
    std::vector<VkAttachmentDescription> attachments;

    // Color attachment
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = config.colorFormat;
    colorAttachment.samples = config.samples;
    colorAttachment.loadOp = config.clearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments.push_back(colorAttachment);

    // Depth attachment (if needed)
    VkAttachmentReference depthAttachmentRef = {};
    if (config.hasDepth && config.depthFormat != VK_FORMAT_UNDEFINED) {
        VkAttachmentDescription depthAttachment = {};
        depthAttachment.format = config.depthFormat;
        depthAttachment.samples = config.samples;
        depthAttachment.loadOp = config.clearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachments.push_back(depthAttachment);

        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    // Subpass
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    if (config.hasDepth) {
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
    }

    // Subpass dependencies
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult result = qvkCreateRenderPass(vk.device, &renderPassInfo, nullptr, renderPass);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create render pass: %s\n", vk_result_string(result));
    } else {
        ri.Printf(PRINT_ALL, "Vulkan: Render pass created successfully\n");
    }

    return result;
}

} // namespace render_pass_mgr

// Public interface functions
VkResult vk_create_main_render_pass_enhanced(VkFormat colorFormat, VkFormat depthFormat, VkRenderPass* renderPass) {
    render_pass_mgr::RenderPassConfig config = {};
    config.colorFormat = colorFormat;
    config.depthFormat = depthFormat;
    config.hasDepth = (depthFormat != VK_FORMAT_UNDEFINED);
    config.clearColor = true;
    config.clearDepth = true;
    config.samples = VK_SAMPLE_COUNT_1_BIT;

    return render_pass_mgr::create_basic_render_pass(config, renderPass);
}

#endif // USE_VULKAN