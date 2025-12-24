#include "vk_renderpass.h"
#include "vk_utils.h"
#include "../renderercommon/tr_public.h"
#include "vk.h"

// Renderer interface
extern refimport_t ri;

// Vulkan function pointer extern declarations
extern PFN_vkCreateRenderPass qvkCreateRenderPass;
extern PFN_vkDestroyRenderPass qvkDestroyRenderPass;
extern PFN_vkCreateFramebuffer qvkCreateFramebuffer;
extern PFN_vkDestroyFramebuffer qvkDestroyFramebuffer;
extern PFN_vkCmdBeginRenderPass qvkCmdBeginRenderPass;
extern PFN_vkCmdEndRenderPass qvkCmdEndRenderPass;
extern PFN_vkCmdNextSubpass qvkCmdNextSubpass;

// Utility functions
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

// Clear color values for different render passes
static const VkClearValue default_clear_values[] = {
    {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
    {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
};

// Create main render pass
qboolean vk_create_main_render_pass(void) {
    VkAttachmentDescription attachments[2];
    VkAttachmentReference color_ref, depth_ref;
    VkSubpassDescription subpass;
    VkRenderPassCreateInfo desc;

    // Color attachment
    attachments[0].flags = 0;
    attachments[0].format = vk.color_format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Depth attachment
    attachments[1].flags = 0;
    attachments[1].format = vk.depth_format;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpass references
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    depth_ref.attachment = 1;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpass
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = NULL;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pResolveAttachments = NULL;
    subpass.pDepthStencilAttachment = &depth_ref;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = NULL;

    // Render pass
    desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.attachmentCount = ARRAY_LEN(attachments);
    desc.pAttachments = attachments;
    desc.subpassCount = 1;
    desc.pSubpasses = &subpass;
    desc.dependencyCount = 0;
    desc.pDependencies = NULL;

    VkResult result = qvkCreateRenderPass(vk.device, &desc, NULL, &vk.render_pass.main);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_main_render_pass: Failed to create main render pass: %s\n", vk_result_string(result));
        return qfalse;
    }

    SET_OBJECT_NAME(vk.render_pass.main, "main render pass", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
    ri.Printf(PRINT_ALL, "Vulkan: Main render pass created successfully\n");
    return qtrue;
}

// Create screen map render pass
qboolean vk_create_screenmap_render_pass(void) {
    VkAttachmentDescription attachment;
    VkAttachmentReference color_ref;
    VkSubpassDescription subpass;
    VkRenderPassCreateInfo desc;

    // Color attachment (screen map)
    attachment.flags = 0;
    attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Subpass reference
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Subpass
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = NULL;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pResolveAttachments = NULL;
    subpass.pDepthStencilAttachment = NULL;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = NULL;

    // Render pass
    desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.attachmentCount = 1;
    desc.pAttachments = &attachment;
    desc.subpassCount = 1;
    desc.pSubpasses = &subpass;
    desc.dependencyCount = 0;
    desc.pDependencies = NULL;

    VkResult result = qvkCreateRenderPass(vk.device, &desc, NULL, &vk.render_pass.screenmap);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_screenmap_render_pass: Failed to create screenmap render pass: %s\n", vk_result_string(result));
        return qfalse;
    }

    SET_OBJECT_NAME(vk.render_pass.screenmap, "screenmap render pass", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
    ri.Printf(PRINT_ALL, "Vulkan: Screenmap render pass created successfully\n");
    return qtrue;
}

// Create framebuffer
VkFramebuffer vk_create_framebuffer(VkRenderPass render_pass, uint32_t attachment_count,
    const VkImageView* attachments, uint32_t width, uint32_t height) {

    if (!vk_validate_handle(render_pass, "render pass")) {
        return VK_NULL_HANDLE;
    }

    VkFramebufferCreateInfo desc = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = render_pass,
        .attachmentCount = attachment_count,
        .pAttachments = attachments,
        .width = width,
        .height = height,
        .layers = 1
    };

    VkFramebuffer framebuffer;
    VkResult result = qvkCreateFramebuffer(vk.device, &desc, NULL, &framebuffer);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_framebuffer: Failed to create framebuffer: %s\n", vk_result_string(result));
        return VK_NULL_HANDLE;
    }

    return framebuffer;
}

// Destroy framebuffer
void vk_destroy_framebuffer(VkFramebuffer framebuffer) {
    if (framebuffer != VK_NULL_HANDLE) {
        qvkDestroyFramebuffer(vk.device, framebuffer, NULL);
    }
}

// Destroy render pass
void vk_destroy_render_pass(VkRenderPass render_pass) {
    if (render_pass != VK_NULL_HANDLE) {
        qvkDestroyRenderPass(vk.device, render_pass, NULL);
    }
}

// Begin render pass
void vk_begin_specific_render_pass(VkRenderPass render_pass, VkFramebuffer framebuffer,
    qboolean clear_values, uint32_t width, uint32_t height) {

    if (!vk_validate_handle(render_pass, "render pass") ||
        !vk_validate_handle(framebuffer, "framebuffer")) {
        return;
    }

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = render_pass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = {width, height}
        },
        .clearValueCount = clear_values ? ARRAY_LEN(default_clear_values) : 0,
        .pClearValues = clear_values ? default_clear_values : NULL
    };

    qvkCmdBeginRenderPass(vk.cmd->command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}

// End render pass

// Transition to next subpass
void vk_next_subpass(void) {
    qvkCmdNextSubpass(vk.cmd->command_buffer, VK_SUBPASS_CONTENTS_INLINE);
}

// Begin bloom extract render pass

// Begin blur render pass

// Barrier for final image to shader read
