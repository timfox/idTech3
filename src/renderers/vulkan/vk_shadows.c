#include "vk_shadows.h"
#include "vk.h"
#include "tr_local.h"

vk_shadow_t vk_shadow;

void VK_Shadows_Init(void) {
    memset(&vk_shadow, 0, sizeof(vk_shadow_t));

    vk_shadow.technique = SHADOW_DEPTH_MAP;
    vk_shadow.enabled = qtrue;
    vk_shadow.shadowBias = 0.005f;
    vk_shadow.shadowSlopeBias = 0.01f;
    vk_shadow.shadowNear = 1.0f;
    vk_shadow.shadowFar = 1000.0f;
    vk_shadow.shadowFilterSize = 1.0f;
    vk_shadow.shadowMapSize = 2048;

    vk_shadow.cascadeCount = 4;
    VectorSet(vk_shadow.lightDirection, -0.5f, -0.5f, -1.0f);
    VectorSet(vk_shadow.lightColor, 1.0f, 1.0f, 1.0f);
    vk_shadow.lightColor[3] = 1.0f;
    vk_shadow.lightIntensity = 1.0f;

    // Initialize based on technique
    switch (vk_shadow.technique) {
        case SHADOW_DEPTH_MAP:
            VK_Shadows_InitDepthMap();
            break;
        case SHADOW_CSM:
            VK_Shadows_InitCSM();
            break;
        case SHADOW_VSM:
            VK_Shadows_InitVSM();
            break;
        default:
            break;
    }

    vk_shadow.initialized = qtrue;
    ri.Printf(PRINT_ALL, "Shadow system initialized (%s)\n", VK_Shadows_GetTechniqueName());
}

void VK_Shadows_Shutdown(void) {
    if (!vk_shadow.initialized) return;

    // Free resources based on technique
    switch (vk_shadow.technique) {
        case SHADOW_DEPTH_MAP:
            VK_Shadows_ShutdownDepthMap();
            break;
        case SHADOW_CSM:
            VK_Shadows_ShutdownCSM();
            break;
        case SHADOW_VSM:
            VK_Shadows_ShutdownVSM();
            break;
        default:
            break;
    }

    memset(&vk_shadow, 0, sizeof(vk_shadow_t));
}

void VK_Shadows_InitDepthMap(void) {
    // Create depth image
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.extent.width = vk_shadow.shadowMapSize;
    imageInfo.extent.height = vk_shadow.shadowMapSize;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(qvkCreateImage(vk.device, &imageInfo, NULL, &vk_shadow.depthImage));

    VkMemoryRequirements memReq;
    qvkGetImageMemoryRequirements(vk.device, vk_shadow.depthImage, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk_shadow.depthMemory));
    VK_CHECK(qvkBindImageMemory(vk.device, vk_shadow.depthImage, vk_shadow.depthMemory, 0));

    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vk_shadow.depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(qvkCreateImageView(vk.device, &viewInfo, NULL, &vk_shadow.depthView));

    // Create render pass
    VkAttachmentDescription attachment = {};
    attachment.format = VK_FORMAT_D32_SFLOAT;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef = {};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthRef;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    VK_CHECK(qvkCreateRenderPass(vk.device, &renderPassInfo, NULL, &vk_shadow.renderPass));

    // Create framebuffer
    VkFramebufferCreateInfo fbInfo = {};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = vk_shadow.renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &vk_shadow.depthView;
    fbInfo.width = vk_shadow.shadowMapSize;
    fbInfo.height = vk_shadow.shadowMapSize;
    fbInfo.layers = 1;

    VK_CHECK(qvkCreateFramebuffer(vk.device, &fbInfo, NULL, &vk_shadow.framebuffer));
}

void VK_Shadows_InitCSM(void) {
    vk_shadow.cascadeCount = 4;

    // Calculate cascade splits (practical split scheme)
    vk_shadow.cascadeSplits[0] = vk_shadow.shadowNear;
    vk_shadow.cascadeSplits[1] = 25.0f;
    vk_shadow.cascadeSplits[2] = 100.0f;
    vk_shadow.cascadeSplits[3] = 500.0f;

    // Create depth images for each cascade
    for (int i = 0; i < vk_shadow.cascadeCount; i++) {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_D32_SFLOAT;
        imageInfo.extent.width = vk_shadow.shadowMapSize;
        imageInfo.extent.height = vk_shadow.shadowMapSize;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(qvkCreateImage(vk.device, &imageInfo, NULL, &vk_shadow.csmDepthImages[i]));

        VkMemoryRequirements memReq;
        qvkGetImageMemoryRequirements(vk.device, vk_shadow.csmDepthImages[i], &memReq);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk_shadow.csmDepthMemory[i]));
        VK_CHECK(qvkBindImageMemory(vk.device, vk_shadow.csmDepthImages[i], vk_shadow.csmDepthMemory[i], 0));

        // Create image view
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = vk_shadow.csmDepthImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(qvkCreateImageView(vk.device, &viewInfo, NULL, &vk_shadow.csmDepthViews[i]));

        // Create framebuffer for each cascade
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = vk_shadow.renderPass; // Reuse from depth map
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &vk_shadow.csmDepthViews[i];
        fbInfo.width = vk_shadow.shadowMapSize;
        fbInfo.height = vk_shadow.shadowMapSize;
        fbInfo.layers = 1;

        VK_CHECK(qvkCreateFramebuffer(vk.device, &fbInfo, NULL, &vk_shadow.csmFramebuffers[i]));
    }
}

void VK_Shadows_InitVSM(void) {
    // Create RGBA32F texture for variance (mean + variance)
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R32G32_SFLOAT; // RG = mean + variance
    imageInfo.extent.width = vk_shadow.shadowMapSize;
    imageInfo.extent.height = vk_shadow.shadowMapSize;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(qvkCreateImage(vk.device, &imageInfo, NULL, &vk_shadow.vsmImage));

    VkMemoryRequirements memReq;
    qvkGetImageMemoryRequirements(vk.device, vk_shadow.vsmImage, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk_shadow.vsmMemory));
    VK_CHECK(qvkBindImageMemory(vk.device, vk_shadow.vsmImage, vk_shadow.vsmMemory, 0));

    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vk_shadow.vsmImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(qvkCreateImageView(vk.device, &viewInfo, NULL, &vk_shadow.vsmView));
}

void VK_Shadows_ShutdownDepthMap(void) {
    if (vk_shadow.framebuffer != VK_NULL_HANDLE) {
        qvkDestroyFramebuffer(vk.device, vk_shadow.framebuffer, NULL);
    }
    if (vk_shadow.renderPass != VK_NULL_HANDLE) {
        qvkDestroyRenderPass(vk.device, vk_shadow.renderPass, NULL);
    }
    if (vk_shadow.depthView != VK_NULL_HANDLE) {
        qvkDestroyImageView(vk.device, vk_shadow.depthView, NULL);
    }
    if (vk_shadow.depthImage != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk_shadow.depthImage, NULL);
    }
    if (vk_shadow.depthMemory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk_shadow.depthMemory, NULL);
    }
}

void VK_Shadows_ShutdownCSM(void) {
    for (int i = 0; i < vk_shadow.cascadeCount; i++) {
        if (vk_shadow.csmFramebuffers[i] != VK_NULL_HANDLE) {
            qvkDestroyFramebuffer(vk.device, vk_shadow.csmFramebuffers[i], NULL);
        }
        if (vk_shadow.csmDepthViews[i] != VK_NULL_HANDLE) {
            qvkDestroyImageView(vk.device, vk_shadow.csmDepthViews[i], NULL);
        }
        if (vk_shadow.csmDepthImages[i] != VK_NULL_HANDLE) {
            qvkDestroyImage(vk.device, vk_shadow.csmDepthImages[i], NULL);
        }
        if (vk_shadow.csmDepthMemory[i] != VK_NULL_HANDLE) {
            qvkFreeMemory(vk.device, vk_shadow.csmDepthMemory[i], NULL);
        }
    }
}

void VK_Shadows_ShutdownVSM(void) {
    if (vk_shadow.vsmView != VK_NULL_HANDLE) {
        qvkDestroyImageView(vk.device, vk_shadow.vsmView, NULL);
    }
    if (vk_shadow.vsmImage != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk_shadow.vsmImage, NULL);
    }
    if (vk_shadow.vsmMemory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk_shadow.vsmMemory, NULL);
    }
}

void VK_Shadows_BeginFrame(void) {
    // Update shadow parameters for current frame
    // This would update light matrices, etc.
}

void VK_Shadows_RenderDepth(const refdef_t* refdef) {
    if (!vk_shadow.enabled || !vk_shadow.initialized) return;

    switch (vk_shadow.technique) {
        case SHADOW_DEPTH_MAP:
            VK_Shadows_RenderDepthMap(refdef);
            break;
        case SHADOW_CSM:
            VK_Shadows_RenderCSMAll(refdef);
            break;
        case SHADOW_VSM:
            VK_Shadows_RenderVSM(refdef);
            break;
        default:
            break;
    }
}

void VK_Shadows_RenderDepthMap(const refdef_t* refdef) {
    // Shadow rendering will be integrated into the main render loop
    // This function will be called from vk_begin_frame or similar
    // For now, this is a placeholder that will be filled in when integrating
    // with the main rendering pipeline
    Q_UNUSED(refdef);
}

void VK_Shadows_RenderCSMAll(const refdef_t* refdef) {
    VK_Shadows_UpdateCSMSplits(refdef);

    for (int i = 0; i < vk_shadow.cascadeCount; i++) {
        VK_Shadows_RenderCSM(refdef, i);
    }
}

void VK_Shadows_UpdateCSMSplits(const refdef_t* refdef) {
    // Update cascade split distances based on camera
    float nearClip = refdef->rdflags & RDF_NOWORLDMODEL ? 1.0f : r_znear->value;
    float farClip = refdef->rdflags & RDF_NOWORLDMODEL ? 1024.0f : r_zfar->value;

    // Practical split scheme - focus more detail on near objects
    vk_shadow.cascadeSplits[0] = nearClip;
    vk_shadow.cascadeSplits[1] = nearClip + (farClip - nearClip) * 0.1f;
    vk_shadow.cascadeSplits[2] = nearClip + (farClip - nearClip) * 0.3f;
    vk_shadow.cascadeSplits[3] = farClip;
}

void VK_Shadows_RenderCSM(const refdef_t* refdef, int cascadeIndex) {
    // Render specific cascade
    // This would set up the light's view-projection matrix for this cascade
    // and render all shadow casters into the cascade's depth buffer
    Q_UNUSED(refdef);
    Q_UNUSED(cascadeIndex);
}

void VK_Shadows_RenderVSM(const refdef_t* refdef) {
    // Render variance shadow map
    // This would render depth moments (depth and depth^2) into RG texture
    Q_UNUSED(refdef);
}

void VK_Shadows_EndFrame(void) {
    // Clean up shadow rendering for this frame
}

void VK_Shadows_SetTechnique(shadowTechnique_t technique) {
    if (vk_shadow.technique == technique) return;

    // Shutdown current technique
    VK_Shadows_Shutdown();

    // Set new technique and reinitialize
    vk_shadow.technique = technique;
    VK_Shadows_Init();
}

void VK_Shadows_SetLightDirection(const vec3_t direction) {
    VectorCopy(direction, vk_shadow.lightDirection);
    VectorNormalize(vk_shadow.lightDirection);
}

void VK_Shadows_SetLightColor(const vec4_t color) {
    VectorCopy(color, vk_shadow.lightColor);
}

float VK_Shadows_GetVisibility(const vec3_t position, float ndotl) {
    if (!vk_shadow.enabled || !vk_shadow.initialized) return 1.0f;

    switch (vk_shadow.technique) {
        case SHADOW_DEPTH_MAP:
            return VK_Shadows_SampleDepthMap(position, ndotl);
        case SHADOW_CSM:
            return VK_Shadows_SampleCSM(position, ndotl);
        case SHADOW_VSM:
            return VK_Shadows_SampleVSM(position);
        case SHADOW_PCF:
            return VK_Shadows_SamplePCF(position, 4);
        case SHADOW_PCSS:
            return VK_Shadows_SamplePCSS(position);
        default:
            return 1.0f;
    }
}

float VK_Shadows_SampleDepthMap(const vec3_t position, float ndotl) {
    // Sample basic depth map
    // This would transform position to shadow space and compare with depth
    Q_UNUSED(position);
    Q_UNUSED(ndotl);
    return 1.0f; // Placeholder - fully lit
}

float VK_Shadows_SampleCSM(const vec3_t position, float ndotl) {
    // Sample cascaded shadow maps
    // Determine which cascade the position falls into and sample accordingly
    Q_UNUSED(position);
    Q_UNUSED(ndotl);
    return 1.0f; // Placeholder
}

float VK_Shadows_SampleVSM(const vec3_t position) {
    // Sample variance shadow map
    // Use Chebyshev's inequality for soft shadows
    Q_UNUSED(position);
    return 1.0f; // Placeholder
}

float VK_Shadows_SamplePCF(const vec3_t position, int samples) {
    // Sample with percentage closer filtering
    // Average multiple shadow samples for soft edges
    Q_UNUSED(position);
    Q_UNUSED(samples);
    return 1.0f; // Placeholder
}

float VK_Shadows_SamplePCSS(const vec3_t position) {
    // Sample with percentage closer soft shadows
    // Adaptive kernel size based on distance to blocker
    Q_UNUSED(position);
    return 1.0f; // Placeholder
}

const char* VK_Shadows_GetTechniqueName(void) {
    switch (vk_shadow.technique) {
        case SHADOW_DISABLED: return "Disabled";
        case SHADOW_STENCIL_VOLUME: return "Stencil Volumes";
        case SHADOW_DEPTH_MAP: return "Depth Map";
        case SHADOW_CSM: return "Cascaded Shadow Maps";
        case SHADOW_VSM: return "Variance Shadow Maps";
        case SHADOW_PCF: return "PCF";
        case SHADOW_PCSS: return "PCSS";
        case SHADOW_MSM: return "Moment Shadow Maps";
        case SHADOW_RSM: return "Reflective Shadow Maps";
        default: return "Unknown";
    }
}