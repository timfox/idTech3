#include "tr_local.h"
#include "vk_renderpass.h"
#include "vk_utils.h"
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
extern "C" void vk_destroy_framebuffer(VkFramebuffer framebuffer) {
    if (framebuffer != VK_NULL_HANDLE) {
        qvkDestroyFramebuffer(vk.device, framebuffer, NULL);
    }
}

// Destroy render pass
extern "C" void vk_destroy_render_pass(VkRenderPass render_pass) {
    if (render_pass != VK_NULL_HANDLE) {
        qvkDestroyRenderPass(vk.device, render_pass, NULL);
    }
}

// Begin render pass
extern "C" void vk_begin_specific_render_pass(VkRenderPass render_pass, VkFramebuffer framebuffer,
    qboolean clear_values, uint32_t width, uint32_t height) {

    if (!vk_validate_handle(render_pass, "render pass") ||
        !vk_validate_handle(framebuffer, "framebuffer")) {
        return;
    }

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = render_pass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = {width, height}
        },
        .clearValueCount = static_cast<uint32_t>(clear_values ? ARRAY_LEN(default_clear_values) : 0),
        .pClearValues = clear_values ? default_clear_values : nullptr
    };

    qvkCmdBeginRenderPass(vk.cmd->command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}

// End render pass
extern "C" void vk_end_render_pass(void) {
    qvkCmdEndRenderPass(vk.cmd->command_buffer);

    // End profiling for current render pass
    // Note: In a more sophisticated implementation, we would track the current pass
    // For now, we'll profile the most recent pass that was started
    if (vk.render_profiler.current_pass_count > 0) {
        vk_render_pass_profile_t *pass = &vk.render_profiler.current_passes[vk.render_profiler.current_pass_count - 1];
        if (pass->end_time == 0) { // Not yet ended
            vk_profile_pass_end(pass->name, pass->draw_calls, pass->vertices_submitted);
        }
    }
}

// Transition to next subpass
extern "C" void vk_next_subpass(void) {
    qvkCmdNextSubpass(vk.cmd->command_buffer, VK_SUBPASS_CONTENTS_INLINE);
}

// Begin bloom extract render pass
extern "C" void vk_begin_bloom_extract_render_pass(void) {
    if (!vk_validate_handle(vk.render_pass.bloom_extract, "bloom extract render pass") ||
        !vk_validate_handle(vk.framebuffers.bloom_extract, "bloom extract framebuffer")) {
        return;
    }

    // Start profiling for bloom extract pass
    vk_profile_pass_start("BloomExtract", 1);

    VkClearValue clear_value = {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}};
    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = vk.render_pass.bloom_extract,
        .framebuffer = vk.framebuffers.bloom_extract,
        .renderArea = {
            .offset = {0, 0},
            .extent = {vk.renderWidth, vk.renderHeight}
        },
        .clearValueCount = 1,
        .pClearValues = &clear_value
    };

    qvkCmdBeginRenderPass(vk.cmd->command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}

// Begin blur render pass
extern "C" void vk_begin_blur_render_pass(uint32_t index) {
    if (!vk_bounds_check(index, VK_NUM_BLOOM_PASSES*2, "blur render pass") ||
        !vk_bounds_check(index, VK_NUM_BLOOM_PASSES*2, "blur framebuffer")) {
        return;
    }

    VkClearValue clear_value = {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}};
    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = vk.render_pass.blur[index],
        .framebuffer = vk.framebuffers.blur[index],
        .renderArea = {
            .offset = {0, 0},
            .extent = {vk.renderWidth, vk.renderHeight}
        },
        .clearValueCount = 1,
        .pClearValues = &clear_value
    };

    qvkCmdBeginRenderPass(vk.cmd->command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}

// Begin main render pass
extern "C" void vk_begin_main_render_pass(void) {
    if (!vk_validate_handle(vk.render_pass.main, "main render pass") ||
        !vk_validate_handle(vk.framebuffers.main[vk.cmd->swapchain_image_index], "main framebuffer")) {
        return;
    }

    VkClearValue clear_values[2];
    clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clear_values[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = vk.render_pass.main,
        .framebuffer = vk.framebuffers.main[vk.cmd->swapchain_image_index],
        .renderArea = {
            .offset = {0, 0},
            .extent = {vk.renderWidth, vk.renderHeight}
        },
        .clearValueCount = 2,
        .pClearValues = clear_values
    };

    qvkCmdBeginRenderPass(vk.cmd->command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}

// Barrier for final image to shader read
extern "C" void vk_barrier_final_image_to_shader_read(VkImage image) {
    if (!vk_validate_handle(image, "image")) {
        return;
    }

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);
}
