#include "vk_framebuffer.h"
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

// Utility functions
extern void *Com_Memcpy(void *dest, const void *src, size_t count);
extern void *Com_Memset(void *dest, int c, size_t count);

// Forward declarations for external structures
extern cvar_t *r_fbo;

// Object naming macro
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

// Forward declarations for utility functions
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);
extern void VK_ImGui_NotifyRenderPassChanged(void);

// Helper to create a single attachment render pass
static VkRenderPass create_simple_render_pass(VkFormat format, VkImageLayout final_layout) {
    VkAttachmentDescription attachment = {
        .flags = 0,
        .format = format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = final_layout
    };

    VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pResolveAttachments = NULL,
        .pDepthStencilAttachment = NULL,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL
    };

    VkRenderPassCreateInfo desc = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .attachmentCount = 1,
        .pAttachments = &attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0,
        .pDependencies = NULL
    };

    VkRenderPass rp;
    VkResult result = qvkCreateRenderPass(vk.device, &desc, NULL, &rp);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "create_simple_render_pass: Failed to create render pass: %s\n", vk_result_string(result));
        return VK_NULL_HANDLE;
    }
    return rp;
}

// Render pass creation
void vk_create_render_passes(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Creating render passes...\n");

    // 1. Main Render Pass (Color + Depth)
    if (!vk_create_main_render_pass()) {
        ri.Error(ERR_FATAL, "Vulkan: Failed to create main render pass");
    }

    // 2. Screenmap Render Pass
    if (!vk_create_screenmap_render_pass()) {
        ri.Error(ERR_FATAL, "Vulkan: Failed to create screenmap render pass");
    }

    // 3. Bloom Extract Render Pass
    vk.render_pass.bloom_extract = create_simple_render_pass(vk.bloom_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    SET_OBJECT_NAME(vk.render_pass.bloom_extract, "bloom extract render pass", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);

    // 4. Blur Render Passes (Downsample/Upsample chain)
    for (int i = 0; i < 4; i++) {
        vk.render_pass.blur[i] = create_simple_render_pass(vk.bloom_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        char name[64];
        Com_sprintf(name, sizeof(name), "blur render pass %d", i);
        SET_OBJECT_NAME(vk.render_pass.blur[i], name, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
    }

    // 5. Post Bloom Render Pass (Final composite before UI)
    vk.render_pass.post_bloom = create_simple_render_pass(vk.color_format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    SET_OBJECT_NAME(vk.render_pass.post_bloom, "post bloom render pass", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);

    // 6. Cubemap Render Pass (for PBR reflections)
    {
        VkAttachmentDescription attachments[2];
        // Color
        attachments[0] = (VkAttachmentDescription){
            .format = vk.color_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };
        // Depth
        attachments[1] = (VkAttachmentDescription){
            .format = vk.depth_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

        VkAttachmentReference color_ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depth_ref = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_ref,
            .pDepthStencilAttachment = &depth_ref
        };

        VkRenderPassCreateInfo desc = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 2,
            .pAttachments = attachments,
            .subpassCount = 1,
            .pSubpasses = &subpass
        };

        VK_CHECK(qvkCreateRenderPass(vk.device, &desc, NULL, &vk.render_pass.cubemap));
        SET_OBJECT_NAME(vk.render_pass.cubemap, "cubemap render pass", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
    }

    // 7. BRDF LUT Render Pass
    vk.render_pass.brdflut = create_simple_render_pass(VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    SET_OBJECT_NAME(vk.render_pass.brdflut, "BRDF LUT render pass", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);

    // 8. Gamma/UI Render Pass (Final swapchain output)
    vk.render_pass.gamma = create_simple_render_pass(vk.present_format.format, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    SET_OBJECT_NAME(vk.render_pass.gamma, "gamma render pass", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);

    ri.Printf(PRINT_ALL, "Vulkan: All render passes created successfully\n");
}

// Framebuffer creation
void vk_create_framebuffers(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Creating framebuffers (%dx%d)...\n", vk.renderWidth, vk.renderHeight);

    // 1. Main Framebuffers (Swapchain dependent)
    for (uint32_t i = 0; i < vk.swapchain_image_count; i++) {
        VkImageView attachments[] = {
            vk.color_image_view,
            vk.depth_image_view
        };
        vk.framebuffers.main[i] = vk_create_framebuffer(vk.render_pass.main, ARRAY_LEN(attachments), attachments, vk.renderWidth, vk.renderHeight);
        char name[64];
        Com_sprintf(name, sizeof(name), "main framebuffer %d", i);
        SET_OBJECT_NAME(vk.framebuffers.main[i], name, VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
    }

    // 2. Screenmap Framebuffer
    {
        VkImageView attachment = vk.screenMap.color_image_view;
        vk.framebuffers.screenmap = vk_create_framebuffer(vk.render_pass.screenmap, 1, &attachment, vk.screenMapWidth, vk.screenMapHeight);
        SET_OBJECT_NAME(vk.framebuffers.screenmap, "screenmap framebuffer", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
    }

    // 3. Bloom Extract Framebuffer
    {
        VkImageView attachment = vk.bloom_image_view[0];
        vk.framebuffers.bloom_extract = vk_create_framebuffer(vk.render_pass.bloom_extract, 1, &attachment, vk.renderWidth / 2, vk.renderHeight / 2);
        SET_OBJECT_NAME(vk.framebuffers.bloom_extract, "bloom extract framebuffer", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
    }

    // 4. Blur Framebuffers
    for (int i = 0; i < 4; i++) {
        VkImageView attachment = vk.bloom_image_view[i + 1];
        uint32_t w = (vk.renderWidth / 2) >> (i + 1);
        uint32_t h = (vk.renderHeight / 2) >> (i + 1);
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        vk.framebuffers.blur[i] = vk_create_framebuffer(vk.render_pass.blur[i], 1, &attachment, w, h);
        char name[64];
        Com_sprintf(name, sizeof(name), "blur framebuffer %d", i);
        SET_OBJECT_NAME(vk.framebuffers.blur[i], name, VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
    }

    // 5. Cubemap Framebuffers
    for (int i = 0; i < 6; i++) {
        if (vk.cubeMap.face_views[i] != VK_NULL_HANDLE) {
            VkImageView attachments[] = {
                vk.cubeMap.face_views[i],
                vk.depth_image_view // Reuse main depth for cubemap faces if size matches, or use dedicated if needed
            };
            vk.framebuffers.cubemap[i] = vk_create_framebuffer(vk.render_pass.cubemap, ARRAY_LEN(attachments), attachments, REF_CUBEMAP_SIZE, REF_CUBEMAP_SIZE);
            char name[64];
            Com_sprintf(name, sizeof(name), "cubemap face framebuffer %d", i);
            SET_OBJECT_NAME(vk.framebuffers.cubemap[i], name, VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
        }
    }

    // 6. BRDF LUT Framebuffer
    if (vk.brdflut.view != VK_NULL_HANDLE) {
        VkImageView attachment = vk.brdflut.view;
        vk.framebuffers.brdflut = vk_create_framebuffer(vk.render_pass.brdflut, 1, &attachment, 512, 512);
        SET_OBJECT_NAME(vk.framebuffers.brdflut, "BRDF LUT framebuffer", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
    }

    VK_ImGui_NotifyRenderPassChanged();
    ri.Printf(PRINT_ALL, "Vulkan: All framebuffers created successfully\n");
}

// Framebuffer destruction
void vk_destroy_framebuffers(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Destroying framebuffers and render passes...\n");

    for (uint32_t i = 0; i < vk.swapchain_image_count; i++) {
        vk_destroy_framebuffer(vk.framebuffers.main[i]);
        vk.framebuffers.main[i] = VK_NULL_HANDLE;
    }

    vk_destroy_framebuffer(vk.framebuffers.screenmap);
    vk.framebuffers.screenmap = VK_NULL_HANDLE;

    vk_destroy_framebuffer(vk.framebuffers.bloom_extract);
    vk.framebuffers.bloom_extract = VK_NULL_HANDLE;

    for (int i = 0; i < 4; i++) {
        vk_destroy_framebuffer(vk.framebuffers.blur[i]);
        vk.framebuffers.blur[i] = VK_NULL_HANDLE;
    }

    for (int i = 0; i < 6; i++) {
        vk_destroy_framebuffer(vk.framebuffers.cubemap[i]);
        vk.framebuffers.cubemap[i] = VK_NULL_HANDLE;
    }

    vk_destroy_framebuffer(vk.framebuffers.brdflut);
    vk.framebuffers.brdflut = VK_NULL_HANDLE;

    // Destroy render passes
    vk_destroy_render_pass(vk.render_pass.main);
    vk.render_pass.main = VK_NULL_HANDLE;

    vk_destroy_render_pass(vk.render_pass.screenmap);
    vk.render_pass.screenmap = VK_NULL_HANDLE;

    vk_destroy_render_pass(vk.render_pass.bloom_extract);
    vk.render_pass.bloom_extract = VK_NULL_HANDLE;

    for (int i = 0; i < 4; i++) {
        vk_destroy_render_pass(vk.render_pass.blur[i]);
        vk.render_pass.blur[i] = VK_NULL_HANDLE;
    }

    vk_destroy_render_pass(vk.render_pass.post_bloom);
    vk.render_pass.post_bloom = VK_NULL_HANDLE;

    vk_destroy_render_pass(vk.render_pass.cubemap);
    vk.render_pass.cubemap = VK_NULL_HANDLE;

    vk_destroy_render_pass(vk.render_pass.brdflut);
    vk.render_pass.brdflut = VK_NULL_HANDLE;

    vk_destroy_render_pass(vk.render_pass.gamma);
    vk.render_pass.gamma = VK_NULL_HANDLE;
}

// Prefilter framebuffer creation for PBR
void vk_create_prefilter_framebuffer(filterDef *def) {
    if (def == NULL) return;

    ri.Printf(PRINT_ALL, "Vulkan: Creating prefilter framebuffer for PBR (target %d)\n", def->target);

    // Use specific offscreen render pass if defined in filterDef
    VkRenderPass rp = def->offscreen.renderpass;
    if (rp == VK_NULL_HANDLE) {
        rp = vk.render_pass.cubemap;
    }

    VkImageView attachments[1] = { def->offscreen.view };
    def->offscreen.framebuffer = vk_create_framebuffer(rp, 1, attachments, def->size, def->size);
    SET_OBJECT_NAME(def->offscreen.framebuffer, "prefilter offscreen framebuffer", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
}

// Render pass transitions
static const VkClearValue main_clear_values[] = {
    {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}}, // Color clear
    {.depthStencil = {1.0f, 0}}            // Depth clear
};

void vk_begin_main_render_pass(void) {
    if (vk.cmd->swapchain_image_acquired) {
        vk_begin_specific_render_pass(vk.render_pass.main, vk.framebuffers.main[vk.cmd->swapchain_image_index], qtrue, vk.renderWidth, vk.renderHeight);
    }
}

void vk_end_main_render_pass(void) {
    vk_end_render_pass();
}

void vk_begin_post_bloom_render_pass(void) {
    // Transition to final composite
    // This typically draws a full-screen quad with bloom added to the main scene
    vk_begin_specific_render_pass(vk.render_pass.post_bloom, vk.framebuffers.main[vk.cmd->swapchain_image_index], qfalse, vk.renderWidth, vk.renderHeight);
}

void vk_end_post_bloom_render_pass(void) {
    vk_end_render_pass();
}

// Framebuffer state queries
qboolean vk_has_framebuffers(void) {
    return (qboolean)(vk.swapchain_image_count > 0 && vk.framebuffers.main[0] != VK_NULL_HANDLE);
}

uint32_t vk_get_framebuffer_count(void) {
    return vk.swapchain_image_count;
}
