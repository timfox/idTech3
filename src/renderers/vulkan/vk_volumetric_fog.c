/*
=============================================================================
Volumetric Fog System Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_atmosphere.h"
#include "vk_post_process.h"

#ifdef USE_VULKAN

extern cvar_t *r_volumetricFog;
extern cvar_t *r_volumetricFogSamples;
extern cvar_t *r_volumetricFogScattering;
extern cvar_t *r_volumetricFogAbsorption;
extern cvar_t *r_volumetricFogDensity;
extern cvar_t *r_volumetricFogHeight;
extern cvar_t *r_volumetricFogFalloff;

typedef struct {
    float projectionMatrix[16];
    float viewMatrix[16];
    float invProjectionMatrix[16];
    float invViewMatrix[16];
    float resolution[2];
    float invResolution[2];
    float cameraPos[3];
    float _pad0;
    float lightDir[3];
    float _pad1;
    float lightColor[3];
    float lightIntensity;
    float fogDensity;
    float fogHeight;
    float fogFalloff;
    int numSamples;
    float scattering;
    float absorption;
} volumetric_fog_pc_t;

void vk_volumetric_fog_init(void)
{
    if (!r_volumetricFog->integer) return;

    ri.Printf(PRINT_ALL, "Initializing volumetric fog system...\n");

    // Create output image
    VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageInfo.extent.width = vk.renderWidth;
    imageInfo.extent.height = vk.renderHeight;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK_CHECK(qvkCreateImage(vk.device, &imageInfo, NULL, &vk.atmosphere.volumetricFogImage));

    VkMemoryRequirements memReqs;
    qvkGetImageMemoryRequirements(vk.device, vk.atmosphere.volumetricFogImage, &memReqs);

    VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk.atmosphere.volumetricFogImageMemory));
    qvkBindImageMemory(vk.device, vk.atmosphere.volumetricFogImage, vk.atmosphere.volumetricFogImageMemory, 0);

    VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = vk.atmosphere.volumetricFogImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(qvkCreateImageView(vk.device, &viewInfo, NULL, &vk.atmosphere.volumetricFogImageView));

    // Create descriptor set layout
    VkDescriptorSetLayoutBinding bindings[3] = {};
    
    // depthBuffer
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // noiseTexture
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // outputImage
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = bindings;

    VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.atmosphere.volumetricFogDescriptorLayout));

    // Create pipeline layout
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(volumetric_fog_pc_t);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.atmosphere.volumetricFogDescriptorLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    VK_CHECK(qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk.atmosphere.volumetricFogLayout));

    // Load shader if needed
    if (vk.modules.volumetric_fog_comp == VK_NULL_HANDLE) {
        void *buffer;
        int size = ri.FS_ReadFile("src/renderers/vulkan/shaders/spirv/volumetric_fog_comp.spv", &buffer);
        if (size > 0) {
            vk.modules.volumetric_fog_comp = vk_create_shader_module((const uint8_t*)buffer, size);
            ri.FS_FreeFile(buffer);
        }
    }

    // Create pipeline
    if (vk.modules.volumetric_fog_comp != VK_NULL_HANDLE) {
        vk.atmosphere.volumetricFogPipeline = vk_create_compute_pipeline(vk.modules.volumetric_fog_comp, vk.atmosphere.volumetricFogLayout, "Volumetric Fog");
    } else {
        ri.Printf(PRINT_WARNING, "Volumetric fog compute shader not found!\n");
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo descriptorAllocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    descriptorAllocInfo.descriptorPool = vk.descriptor_pool;
    descriptorAllocInfo.descriptorSetCount = 1;
    descriptorAllocInfo.pSetLayouts = &vk.atmosphere.volumetricFogDescriptorLayout;

    VK_CHECK(qvkAllocateDescriptorSets(vk.device, &descriptorAllocInfo, &vk.atmosphere.volumetricFogDescriptorSet));

    // Update descriptor set
    Vk_Sampler_Def samplerDef = {0};
    samplerDef.vk_min_filter = VK_FILTER_LINEAR;
    samplerDef.vk_mag_filter = VK_FILTER_LINEAR;
    samplerDef.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSampler sampler = vk_find_sampler(&samplerDef);

    VkDescriptorImageInfo depthInfo = {0};
    depthInfo.sampler = sampler;
    depthInfo.imageView = vk.depth_image_view;
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo noiseInfo = {0};
    noiseInfo.sampler = sampler;
    if (vk.atmosphere.noiseTexture) {
        noiseInfo.imageView = vk.atmosphere.noiseTexture->view;
    } else {
        noiseInfo.imageView = tr.whiteImage->view;
    }
    noiseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outputInfo = {0};
    outputInfo.imageView = vk.atmosphere.volumetricFogImageView;
    outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[3] = {0};
    
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = vk.atmosphere.volumetricFogDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &depthInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = vk.atmosphere.volumetricFogDescriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &noiseInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = vk.atmosphere.volumetricFogDescriptorSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[2].pImageInfo = &outputInfo;

    qvkUpdateDescriptorSets(vk.device, 3, writes, 0, NULL);

    // Create composite descriptor set layout
    VkDescriptorSetLayoutBinding compositeBindings[2] = {};
    
    // colorBuffer (Storage Image)
    compositeBindings[0].binding = 0;
    compositeBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    compositeBindings[0].descriptorCount = 1;
    compositeBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // fogBuffer (Sampled Image)
    compositeBindings[1].binding = 1;
    compositeBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    compositeBindings[1].descriptorCount = 1;
    compositeBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo compositeLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    compositeLayoutInfo.bindingCount = 2;
    compositeLayoutInfo.pBindings = compositeBindings;

    VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &compositeLayoutInfo, NULL, &vk.atmosphere.compositeDescriptorLayout));

    // Create composite pipeline layout
    VkPipelineLayoutCreateInfo compositePipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    compositePipelineLayoutInfo.setLayoutCount = 1;
    compositePipelineLayoutInfo.pSetLayouts = &vk.atmosphere.compositeDescriptorLayout;

    VK_CHECK(qvkCreatePipelineLayout(vk.device, &compositePipelineLayoutInfo, NULL, &vk.atmosphere.compositeLayout));

    // Load composite shader
    if (vk.modules.volumetric_fog_composite_comp == VK_NULL_HANDLE) {
        void *buffer;
        int size = ri.FS_ReadFile("src/renderers/vulkan/shaders/spirv/volumetric_fog_composite_comp.spv", &buffer);
        if (size > 0) {
            vk.modules.volumetric_fog_composite_comp = vk_create_shader_module((const uint8_t*)buffer, size);
            ri.FS_FreeFile(buffer);
        }
    }

    // Create composite pipeline
    if (vk.modules.volumetric_fog_composite_comp != VK_NULL_HANDLE) {
        vk.atmosphere.compositePipeline = vk_create_compute_pipeline(vk.modules.volumetric_fog_composite_comp, vk.atmosphere.compositeLayout, "Volumetric Fog Composite");
    }

    // Allocate composite descriptor set
    VkDescriptorSetAllocateInfo compositeDescriptorAllocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    compositeDescriptorAllocInfo.descriptorPool = vk.descriptor_pool;
    compositeDescriptorAllocInfo.descriptorSetCount = 1;
    compositeDescriptorAllocInfo.pSetLayouts = &vk.atmosphere.compositeDescriptorLayout;

    VK_CHECK(qvkAllocateDescriptorSets(vk.device, &compositeDescriptorAllocInfo, &vk.atmosphere.compositeDescriptorSet));

    vk.atmosphere.initialized = qtrue;
    ri.Printf(PRINT_ALL, "Volumetric fog system: Initialized\n");
}

void vk_volumetric_fog_shutdown(void)
{
    if (!vk.atmosphere.initialized) return;

    if (vk.atmosphere.volumetricFogPipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.atmosphere.volumetricFogPipeline, NULL);
        vk.atmosphere.volumetricFogPipeline = VK_NULL_HANDLE;
    }
    if (vk.atmosphere.volumetricFogLayout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk.atmosphere.volumetricFogLayout, NULL);
        vk.atmosphere.volumetricFogLayout = VK_NULL_HANDLE;
    }
    if (vk.atmosphere.volumetricFogDescriptorLayout != VK_NULL_HANDLE) {
        qvkDestroyDescriptorSetLayout(vk.device, vk.atmosphere.volumetricFogDescriptorLayout, NULL);
        vk.atmosphere.volumetricFogDescriptorLayout = VK_NULL_HANDLE;
    }
    if (vk.atmosphere.volumetricFogImage != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk.atmosphere.volumetricFogImage, NULL);
        vk.atmosphere.volumetricFogImage = VK_NULL_HANDLE;
    }
    if (vk.atmosphere.volumetricFogImageView != VK_NULL_HANDLE) {
        qvkDestroyImageView(vk.device, vk.atmosphere.volumetricFogImageView, NULL);
        vk.atmosphere.volumetricFogImageView = VK_NULL_HANDLE;
    }
    if (vk.atmosphere.volumetricFogImageMemory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk.atmosphere.volumetricFogImageMemory, NULL);
        vk.atmosphere.volumetricFogImageMemory = VK_NULL_HANDLE;
    }

    if (vk.atmosphere.compositePipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.atmosphere.compositePipeline, NULL);
        vk.atmosphere.compositePipeline = VK_NULL_HANDLE;
    }
    if (vk.atmosphere.compositeLayout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk.atmosphere.compositeLayout, NULL);
        vk.atmosphere.compositeLayout = VK_NULL_HANDLE;
    }
    if (vk.atmosphere.compositeDescriptorLayout != VK_NULL_HANDLE) {
        qvkDestroyDescriptorSetLayout(vk.device, vk.atmosphere.compositeDescriptorLayout, NULL);
        vk.atmosphere.compositeDescriptorLayout = VK_NULL_HANDLE;
    }

    vk.atmosphere.initialized = qfalse;
}

void vk_volumetric_fog_render(VkCommandBuffer cmdBuffer)
{
    if (!r_volumetricFog->integer || !vk.atmosphere.initialized || vk.atmosphere.volumetricFogPipeline == VK_NULL_HANDLE) return;

    // Transition output image to GENERAL for writing
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vk.atmosphere.volumetricFogImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    qvkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    // Bind pipeline and descriptor sets
    qvkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.atmosphere.volumetricFogPipeline);
    qvkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.atmosphere.volumetricFogLayout, 0, 1, &vk.atmosphere.volumetricFogDescriptorSet, 0, NULL);

    // Prepare push constants
    volumetric_fog_pc_t pc = {0};
    Com_Memcpy(pc.projectionMatrix, vk.cmd->mvp.projection, sizeof(pc.projectionMatrix));
    Com_Memcpy(pc.viewMatrix, vk.cmd->mvp.view, sizeof(pc.viewMatrix));
    // Calculate inverses
    Matrix16Inverse(pc.projectionMatrix, pc.invProjectionMatrix);
    Matrix16Inverse(pc.viewMatrix, pc.invViewMatrix);

    pc.resolution[0] = (float)vk.renderWidth;
    pc.resolution[1] = (float)vk.renderHeight;
    pc.invResolution[0] = 1.0f / (float)vk.renderWidth;
    pc.invResolution[1] = 1.0f / (float)vk.renderHeight;
    
    VectorCopy(backEnd.viewParms.vieworg, pc.cameraPos);
    VectorCopy(tr.sunDirection, pc.lightDir);
    VectorCopy(tr.sunLight, pc.lightColor);
    pc.lightIntensity = 1.0f;

    pc.fogDensity = r_volumetricFogDensity->value;
    pc.fogHeight = r_volumetricFogHeight->value;
    pc.fogFalloff = r_volumetricFogFalloff->value;
    pc.numSamples = r_volumetricFogSamples->integer;
    pc.scattering = r_volumetricFogScattering->value;
    pc.absorption = r_volumetricFogAbsorption->value;

    qvkCmdPushConstants(cmdBuffer, vk.atmosphere.volumetricFogLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

    // Dispatch
    uint32_t groupCountX = (vk.renderWidth + 7) / 8;
    uint32_t groupCountY = (vk.renderHeight + 7) / 8;
    qvkCmdDispatch(cmdBuffer, groupCountX, groupCountY, 1);

    // Transition output image to SHADER_READ_ONLY_OPTIMAL for sampling in composite
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    qvkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_BIT, VK_PIPELINE_STAGE_COMPUTE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    // Transition color image to GENERAL for writing in composite
    VkImageMemoryBarrier colorBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    colorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    colorBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    colorBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.image = vk.color_image;
    colorBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorBarrier.subresourceRange.levelCount = 1;
    colorBarrier.subresourceRange.layerCount = 1;
    colorBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    colorBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

    qvkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_BIT, 0, 0, NULL, 0, NULL, 1, &colorBarrier);

    // Perform composite
    // Update composite descriptor set with latest color image
    Vk_Sampler_Def samplerDef = {0};
    samplerDef.vk_min_filter = VK_FILTER_LINEAR;
    samplerDef.vk_mag_filter = VK_FILTER_LINEAR;
    samplerDef.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSampler sampler = vk_find_sampler(&samplerDef);

    VkDescriptorImageInfo colorInfo = {0};
    colorInfo.sampler = VK_NULL_HANDLE;
    colorInfo.imageView = vk.color_image_view;
    colorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo fogInfo = {0};
    fogInfo.sampler = sampler;
    fogInfo.imageView = vk.atmosphere.volumetricFogImageView;
    fogInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet compositeWrites[2] = {0};
    compositeWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    compositeWrites[0].dstSet = vk.atmosphere.compositeDescriptorSet;
    compositeWrites[0].dstBinding = 0;
    compositeWrites[0].descriptorCount = 1;
    compositeWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    compositeWrites[0].pImageInfo = &colorInfo;

    compositeWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    compositeWrites[1].dstSet = vk.atmosphere.compositeDescriptorSet;
    compositeWrites[1].dstBinding = 1;
    compositeWrites[1].descriptorCount = 1;
    compositeWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    compositeWrites[1].pImageInfo = &fogInfo;

    qvkUpdateDescriptorSets(vk.device, 2, compositeWrites, 0, NULL);

    // Bind composite pipeline and descriptor set
    qvkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.atmosphere.compositePipeline);
    qvkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.atmosphere.compositeLayout, 0, 1, &vk.atmosphere.compositeDescriptorSet, 0, NULL);

    // Dispatch composite
    qvkCmdDispatch(cmdBuffer, groupCountX, groupCountY, 1);

    // Transition color image back to SHADER_READ_ONLY_OPTIMAL
    colorBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    colorBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    colorBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    qvkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &colorBarrier);
}

#endif // USE_VULKAN
