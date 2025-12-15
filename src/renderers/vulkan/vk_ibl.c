#include "vk_ibl.h"
#include "vk.h"
#include "tr_local.h"
#include "vk_material_system.h"

vk_ibl_t vk_ibl;

void VK_IBL_Init(void) {
    memset(&vk_ibl, 0, sizeof(vk_ibl_t));

    // Create descriptor set layout for IBL
    VkDescriptorSetLayoutBinding bindings[4] = {};

    // Irradiance cubemap
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Radiance cubemap
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // BRDF LUT
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Environment cubemap (for reflections)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 4;
    layoutInfo.pBindings = bindings;

    VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk_ibl.descriptorSetLayout));

    // Generate BRDF LUT (precomputed)
    VK_IBL_GenerateBRDFLut();

    vk_ibl.intensity = 1.0f;
    vk_ibl.rotation = 0.0f;
    VectorSet(vk_ibl.tintColor, 1.0f, 1.0f, 1.0f);
    vk_ibl.enabled = qtrue;
    vk_ibl.initialized = qtrue;

    ri.Printf(PRINT_ALL, "IBL system initialized\n");
}

void VK_IBL_Shutdown(void) {
    if (!vk_ibl.initialized) return;

    // Free Vulkan resources
    if (vk_ibl.envCubemap != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk_ibl.envCubemap, NULL);
        qvkDestroyImageView(vk.device, vk_ibl.envCubemapView, NULL);
        qvkFreeMemory(vk.device, vk_ibl.envCubemapMemory, NULL);
    }

    if (vk_ibl.irradianceCubemap != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk_ibl.irradianceCubemap, NULL);
        qvkDestroyImageView(vk.device, vk_ibl.irradianceCubemapView, NULL);
        qvkFreeMemory(vk.device, vk_ibl.irradianceCubemapMemory, NULL);
    }

    if (vk_ibl.radianceCubemap != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk_ibl.radianceCubemap, NULL);
        qvkDestroyImageView(vk.device, vk_ibl.radianceCubemapView, NULL);
        qvkFreeMemory(vk.device, vk_ibl.radianceCubemapMemory, NULL);
    }

    if (vk_ibl.brdfLut != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk_ibl.brdfLut, NULL);
        qvkDestroyImageView(vk.device, vk_ibl.brdfLutView, NULL);
        qvkFreeMemory(vk.device, vk_ibl.brdfLutMemory, NULL);
    }

    if (vk_ibl.descriptorSetLayout != VK_NULL_HANDLE) {
        qvkDestroyDescriptorSetLayout(vk.device, vk_ibl.descriptorSetLayout, NULL);
    }

    if (vk_ibl.computePipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk_ibl.computePipeline, NULL);
        qvkDestroyPipelineLayout(vk.device, vk_ibl.computePipelineLayout, NULL);
        qvkDestroyDescriptorSetLayout(vk.device, vk_ibl.computeDescriptorSetLayout, NULL);
    }

    memset(&vk_ibl, 0, sizeof(vk_ibl_t));
}

void VK_IBL_LoadEnvironment(const char* cubemapName) {
    char cubemapPath[MAX_QPATH];
    Com_sprintf(cubemapPath, sizeof(cubemapPath), "env/%s", cubemapName);

    // Load environment cubemap
    // This would load a HDR environment map and convert it to a cubemap
    // For now, create a simple procedural environment

    VK_IBL_GenerateIrradiance();
    VK_IBL_GenerateRadiance();
    VK_IBL_UpdateDescriptors();

    ri.Printf(PRINT_ALL, "Loaded IBL environment: %s\n", cubemapName);
}

void VK_IBL_GenerateBRDFLut(void) {
    // Create BRDF LUT texture (256x256 RG)
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R16G16_SFLOAT;
    imageInfo.extent.width = 256;
    imageInfo.extent.height = 256;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(qvkCreateImage(vk.device, &imageInfo, NULL, &vk_ibl.brdfLut));

    VkMemoryRequirements memReq;
    qvkGetImageMemoryRequirements(vk.device, vk_ibl.brdfLut, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk_ibl.brdfLutMemory));
    VK_CHECK(qvkBindImageMemory(vk.device, vk_ibl.brdfLut, vk_ibl.brdfLutMemory, 0));

    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vk_ibl.brdfLut;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(qvkCreateImageView(vk.device, &viewInfo, NULL, &vk_ibl.brdfLutView));

    // Generate BRDF LUT using compute shader
    // This would run a compute shader to precompute the BRDF lookup table
    // For now, we'll defer the actual generation to when it's needed
    // The image layout transition will happen during first use

    ri.Printf(PRINT_ALL, "Generated BRDF LUT\n");
}

void VK_IBL_GenerateIrradiance(void) {
    // Generate irradiance cubemap from environment cubemap
    // This would use a compute shader to convolve the environment map
    // For now, create a simple single-color irradiance map

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.extent.width = 32;
    imageInfo.extent.height = 32;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 6; // Cubemap
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VK_CHECK(qvkCreateImage(vk.device, &imageInfo, NULL, &vk_ibl.irradianceCubemap));

    VkMemoryRequirements memReq;
    qvkGetImageMemoryRequirements(vk.device, vk_ibl.irradianceCubemap, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk_ibl.irradianceCubemapMemory));
    VK_CHECK(qvkBindImageMemory(vk.device, vk_ibl.irradianceCubemap, vk_ibl.irradianceCubemapMemory, 0));

    // Create cubemap view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vk_ibl.irradianceCubemap;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    VK_CHECK(qvkCreateImageView(vk.device, &viewInfo, NULL, &vk_ibl.irradianceCubemapView));

    ri.Printf(PRINT_ALL, "Generated irradiance cubemap\n");
}

void VK_IBL_GenerateRadiance(void) {
    // Generate prefiltered radiance cubemap
    // This would use multiple mipmap levels for different roughness values

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.extent.width = 128;
    imageInfo.extent.height = 128;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 5; // Multiple roughness levels
    imageInfo.arrayLayers = 6; // Cubemap
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VK_CHECK(qvkCreateImage(vk.device, &imageInfo, NULL, &vk_ibl.radianceCubemap));

    VkMemoryRequirements memReq;
    qvkGetImageMemoryRequirements(vk.device, vk_ibl.radianceCubemap, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk_ibl.radianceCubemapMemory));
    VK_CHECK(qvkBindImageMemory(vk.device, vk_ibl.radianceCubemap, vk_ibl.radianceCubemapMemory, 0));

    // Create cubemap view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vk_ibl.radianceCubemap;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 5;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    VK_CHECK(qvkCreateImageView(vk.device, &viewInfo, NULL, &vk_ibl.radianceCubemapView));

    ri.Printf(PRINT_ALL, "Generated radiance cubemap\n");
}

void VK_IBL_UpdateDescriptors(void) {
    if (!vk_ibl.initialized) return;

    // Allocate descriptor set if not already done
    if (vk_ibl.descriptorSet == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = vk.descriptor_pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &vk_ibl.descriptorSetLayout;

        VK_CHECK(qvkAllocateDescriptorSets(vk.device, &allocInfo, &vk_ibl.descriptorSet));
    }

    // Update descriptor set
    VkDescriptorImageInfo imageInfos[4] = {};

    // Irradiance
    if (vk_ibl.irradianceCubemapView != VK_NULL_HANDLE) {
        imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[0].imageView = vk_ibl.irradianceCubemapView;
        imageInfos[0].sampler = vk.sampler;
    }

    // Radiance
    if (vk_ibl.radianceCubemapView != VK_NULL_HANDLE) {
        imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[1].imageView = vk_ibl.radianceCubemapView;
        imageInfos[1].sampler = vk.sampler;
    }

    // BRDF LUT
    if (vk_ibl.brdfLutView != VK_NULL_HANDLE) {
        imageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[2].imageView = vk_ibl.brdfLutView;
        imageInfos[2].sampler = vk.sampler;
    }

    // Environment
    if (vk_ibl.envCubemapView != VK_NULL_HANDLE) {
        imageInfos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[3].imageView = vk_ibl.envCubemapView;
        imageInfos[3].sampler = vk.sampler;
    }

    VkWriteDescriptorSet writes[4] = {};
    for (int i = 0; i < 4; i++) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = vk_ibl.descriptorSet;
        writes[i].dstBinding = i;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].descriptorCount = 1;
        writes[i].pImageInfo = &imageInfos[i];
    }

    qvkUpdateDescriptorSets(vk.device, 4, writes, 0, NULL);
}

void VK_IBL_RenderEnvironment(qboolean backgroundOnly) {
    // Render environment cubemap as background
    // This would render a skybox or environment probe
    Q_UNUSED(backgroundOnly);
}

// PBR functions
void VK_PBR_ApplyIBL(const material_params_t* material, vec3_t viewDir, vec3_t normal,
                     vec3_t albedo, float metallic, float roughness, vec3_t result) {
    // Apply IBL lighting based on material properties
    // This would sample from irradiance and radiance cubemaps

    // Simplified: just add some ambient lighting
    VectorScale(albedo, 0.1f, result); // 10% ambient

    // Add material-based effects
    if (material->flags & MATERIAL_WET) {
        // Wet materials reflect more
        VectorMA(result, 0.2f, albedo, result);
    }

    if (material->flags & MATERIAL_MAGICAL) {
        // Magical materials glow
        VectorMA(result, material->magicGlow, material->magicColor, result);
    }

    VectorScale(result, vk_ibl.intensity, result);
}

void VK_PBR_ComputeLighting(vec3_t position, vec3_t normal, vec3_t viewDir,
                           vec3_t albedo, float metallic, float roughness,
                           vec3_t emissive, vec3_t result) {
    // Compute complete PBR lighting
    // This would include direct lighting + IBL

    // Start with emissive
    VectorCopy(emissive, result);

    // Add IBL contribution
    vec3_t iblContribution;
    VK_PBR_ApplyIBL(NULL, viewDir, normal, albedo, metallic, roughness, iblContribution);
    VectorAdd(result, iblContribution, result);

    // Direct lighting would be added here
    // For now, add some basic directional lighting
    vec3_t lightDir = {0.5f, 0.5f, 1.0f};
    VectorNormalize(lightDir);

    float NdotL = DotProduct(normal, lightDir);
    if (NdotL > 0) {
        vec3_t diffuse;
        VectorScale(albedo, NdotL * 0.8f, diffuse); // 80% directional light
        VectorAdd(result, diffuse, result);
    }
}

void VK_PBR_ApplyAnisotropy(vec3_t normal, vec3_t tangent, float anisotropy, vec3_t result) {
    // Apply anisotropic BRDF modifications
    // This would modify the specular lobe shape
    Q_UNUSED(normal);
    Q_UNUSED(tangent);
    Q_UNUSED(anisotropy);
    Q_UNUSED(result);
    // TODO: Implement anisotropy
}

void VK_PBR_ApplySheen(vec3_t sheenColor, float sheen, vec3_t result) {
    // Apply sheen/cloth BRDF
    if (sheen > 0.0f) {
        vec3_t sheenContribution;
        VectorScale(sheenColor, sheen, sheenContribution);
        VectorAdd(result, sheenContribution, result);
    }
}

void VK_PBR_ApplySubsurface(vec3_t subsurfaceColor, float subsurface, vec3_t result) {
    // Apply subsurface scattering
    if (subsurface > 0.0f) {
        vec3_t sssContribution;
        VectorScale(subsurfaceColor, subsurface, sssContribution);
        VectorAdd(result, sssContribution, result);
    }
}

void VK_PBR_ApplyClearcoat(float clearcoat, float clearcoatRoughness, vec3_t result) {
    // Apply clearcoat layer
    if (clearcoat > 0.0f) {
        // Clearcoat adds extra specular reflection
        VectorMA(result, clearcoat, result, result);
    }
    Q_UNUSED(clearcoatRoughness);
}