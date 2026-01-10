#include "tr_local.h"
#include "vk_descriptors.h"
#include "vk_utils.h"
#include "vk.h"
#include "vk_memory.h"

// Renderer interface
extern refimport_t ri;

// BRDF LUT Sampler bootstrap (private to descriptor module)
static VkSampler brdflut_sampler = VK_NULL_HANDLE;
static void ensure_brdf_sampler(void) {
    if (brdflut_sampler != VK_NULL_HANDLE) return;
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.pNext = NULL;
    samplerInfo.flags = 0;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    VK_CHECK(qvkCreateSampler(vk.device, &samplerInfo, NULL, &brdflut_sampler));
}

// Vulkan function pointer extern declarations
extern PFN_vkCreateDescriptorSetLayout qvkCreateDescriptorSetLayout;
extern PFN_vkCreateDescriptorPool qvkCreateDescriptorPool;
extern PFN_vkAllocateDescriptorSets qvkAllocateDescriptorSets;
extern PFN_vkUpdateDescriptorSets qvkUpdateDescriptorSets;
extern PFN_vkCreateSampler qvkCreateSampler;
extern PFN_vkDestroySampler qvkDestroySampler;

// Utility functions
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

// Vulkan function pointer extern declarations
extern PFN_vkCreateSampler qvkCreateSampler;
extern PFN_vkDestroySampler qvkDestroySampler;

// Find or create sampler
VkSampler vk_find_sampler(const Vk_Sampler_Def *def) {
    VkSamplerAddressMode address_mode;
    VkSamplerCreateInfo desc;
    VkSampler sampler;
    VkFilter mag_filter;
    VkFilter min_filter;
    VkSamplerMipmapMode mipmap_mode;
    float maxLod;
    float lodBias;
    int i;

    if (def == NULL) {
        ri.Printf(PRINT_ERROR, "vk_find_sampler: def is NULL!\n");
        return VK_NULL_HANDLE;
    }

    if (vk.device == VK_NULL_HANDLE) {
        ri.Printf(PRINT_ERROR, "vk_find_sampler: Vulkan device is NULL!\n");
        return VK_NULL_HANDLE;
    }

    // Look for sampler among existing samplers.
    for (i = 0; i < vk.samplers.count; i++) {
        const Vk_Sampler_Def *cur_def = &vk.samplers.def[i];
        if (memcmp(cur_def, def, sizeof(*def)) == 0) {
            return vk.samplers.handle[i];
        }
    }

    // Create new sampler.
    if (vk.samplers.count >= MAX_VK_SAMPLERS) {
        ri.Error(ERR_DROP, "vk_find_sampler: MAX_VK_SAMPLERS hit\n");
        return VK_NULL_HANDLE;
    }

    address_mode = def->address_mode;

    if (def->vk_mag_filter == VK_FILTER_NEAREST) {
        mag_filter = VK_FILTER_NEAREST;
    } else if (def->vk_mag_filter == VK_FILTER_LINEAR) {
        mag_filter = VK_FILTER_LINEAR;
    } else {
        ri.Printf(PRINT_ERROR, "vk_find_sampler: invalid vk_mag_filter %d, using VK_FILTER_LINEAR\n", def->vk_mag_filter);
        mag_filter = VK_FILTER_LINEAR;
    }

    maxLod = vk.samplers.maxLod;

    if (def->vk_min_filter == VK_FILTER_NEAREST) {
        min_filter = VK_FILTER_NEAREST;
        mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        maxLod = 0.25f; // used to emulate OpenGL's VK_FILTER_LINEAR/VK_FILTER_NEAREST minification filter
    } else if (def->vk_min_filter == VK_FILTER_LINEAR) {
        min_filter = VK_FILTER_LINEAR;
        mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        maxLod = 0.25f; // used to emulate OpenGL's VK_FILTER_LINEAR/VK_FILTER_NEAREST minification filter
    } else if (def->vk_min_filter == VK_FILTER_NEAREST_MIPMAP_NEAREST) {
        min_filter = VK_FILTER_NEAREST;
        mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    } else if (def->vk_min_filter == VK_FILTER_LINEAR_MIPMAP_NEAREST) {
        min_filter = VK_FILTER_LINEAR;
        mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    } else if (def->vk_min_filter == VK_FILTER_NEAREST_MIPMAP_LINEAR) {
        min_filter = VK_FILTER_NEAREST;
        mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    } else if (def->vk_min_filter == VK_FILTER_LINEAR_MIPMAP_LINEAR) {
        min_filter = VK_FILTER_LINEAR;
        mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    } else {
        ri.Printf(PRINT_ERROR, "vk_find_sampler: invalid vk_min_filter %d, using VK_FILTER_LINEAR\n", def->vk_min_filter);
        min_filter = VK_FILTER_LINEAR;
        mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        maxLod = 0.25f;
    }

    if (def->max_lod_1_0) {
        maxLod = 1.0f;
    }

    // For font textures without mipmaps, force maxLod=0.0f to only sample from base mip level
    // This prevents blurriness and chunky block artifacts from mipmap sampling
    if (def->isFontTexture && mipmap_mode == VK_SAMPLER_MIPMAP_MODE_NEAREST &&
        (def->vk_min_filter == VK_FILTER_NEAREST || def->vk_min_filter == VK_FILTER_LINEAR)) {
        maxLod = 0.0f;
    }

    lodBias = Com_Clamp(-2.0f, 2.0f, r_textureLodBias ? r_textureLodBias->value : 0.0f);

    desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.magFilter = mag_filter;
    desc.minFilter = min_filter;
    desc.mipmapMode = mipmap_mode;
    desc.addressModeU = address_mode;
    desc.addressModeV = address_mode;
    desc.addressModeW = address_mode;
    desc.mipLodBias = lodBias;

    if (def->noAnisotropy || mipmap_mode == VK_SAMPLER_MIPMAP_MODE_NEAREST || mag_filter == VK_FILTER_NEAREST) {
        desc.anisotropyEnable = VK_FALSE;
        desc.maxAnisotropy = 1.0f;
    } else {
        desc.anisotropyEnable = (r_ext_texture_filter_anisotropic->integer && vk.samplers.samplerAnisotropy) ? VK_TRUE : VK_FALSE;
        if (desc.anisotropyEnable) {
            desc.maxAnisotropy = MIN(r_ext_max_anisotropy->integer, vk.maxAnisotropy);
        }
    }

    desc.compareEnable = VK_FALSE;
    desc.compareOp = VK_COMPARE_OP_ALWAYS;
    desc.minLod = 0.0f;
    desc.maxLod = (maxLod == vk.samplers.maxLod) ? VK_LOD_CLAMP_NONE : maxLod;
    desc.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    desc.unnormalizedCoordinates = VK_FALSE;

    VK_CHECK(qvkCreateSampler(vk.device, &desc, NULL, &sampler));

    SET_OBJECT_NAME(sampler, va("image sampler %i", vk.samplers.count), VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT);

    vk.samplers.def[vk.samplers.count] = *def;
    vk.samplers.handle[vk.samplers.count] = sampler;
    vk.samplers.count++;

    return sampler;
}

// CVAR externs
extern cvar_t *r_bloom;
extern cvar_t *r_vk_hotReload;

// Forward declarations for functions used from other modules
extern VkSampler vk_find_sampler(const Vk_Sampler_Def *def);

// Memory tracking functions
extern void vk_track_allocation(VkDeviceSize size);
extern void vk_track_free(VkDeviceSize size);

// Static function declarations
static void vk_create_descriptor_set_layouts(void);
static void vk_create_pipeline_layouts(void);

// Create descriptor pool
qboolean vk_create_descriptor_pool(void) {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1024 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4096 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 256 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 256 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 256 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 64 }
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 8192,
        .poolSizeCount = ARRAY_LEN(pool_sizes),
        .pPoolSizes = pool_sizes
    };

    VkResult result = qvkCreateDescriptorPool(vk.device, &pool_info, NULL, &vk.descriptor_pool);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_descriptor_pool: Failed to create descriptor pool: %s\n", vk_result_string(result));
        return qfalse;
    }

    SET_OBJECT_NAME(vk.descriptor_pool, "main descriptor pool", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT);

    // Create descriptor set layouts
    vk_create_descriptor_set_layouts();

    // Create pipeline layouts
    vk_create_pipeline_layouts();

    ri.Printf(PRINT_ALL, "Vulkan: Descriptor pool created successfully\n");
    return qtrue;
}

// Create descriptor set layouts
static void vk_create_descriptor_set_layouts(void) {
    // Create sampler layout
    {
        VkDescriptorSetLayoutBinding binding = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        };

        VkDescriptorSetLayoutCreateInfo desc = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .bindingCount = 1,
            .pBindings = &binding
        };

        VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &desc, NULL, &vk.set_layout_sampler));
        SET_OBJECT_NAME(vk.set_layout_sampler, "sampler layout", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT);
    }

    // Create uniform layout
    {
        VkDescriptorSetLayoutBinding bindings[2];

        bindings[0] = VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        };

        bindings[1] = VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        };

        VkDescriptorSetLayoutCreateInfo desc = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .bindingCount = 2,
            .pBindings = bindings
        };

        VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &desc, NULL, &vk.set_layout_uniform));
        SET_OBJECT_NAME(vk.set_layout_uniform, "uniform layout", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT);
    }

    // Create storage layout
    {
        VkDescriptorSetLayoutBinding binding = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        };

        VkDescriptorSetLayoutCreateInfo desc = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .bindingCount = 1,
            .pBindings = &binding
        };

        VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &desc, NULL, &vk.set_layout_storage));
        SET_OBJECT_NAME(vk.set_layout_storage, "storage layout", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT);
    }

    // Create compute storage layout
    {
        VkDescriptorSetLayoutBinding binding = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        };

        VkDescriptorSetLayoutCreateInfo desc = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .bindingCount = 1,
            .pBindings = &binding
        };

        VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &desc, NULL, &vk.compute_descriptor_set_layout));
        SET_OBJECT_NAME(vk.compute_descriptor_set_layout, "compute storage layout", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT);
    }
}

// Create pipeline layouts
static void vk_create_pipeline_layouts(void) {
    // Main pipeline layout
    {
        VkDescriptorSetLayout layouts[3] = {
            vk.set_layout_sampler,
            vk.set_layout_uniform,
            vk.set_layout_storage
        };

        VkPushConstantRange push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(float) * 16 // MVP matrix
        };

        VkPipelineLayoutCreateInfo desc = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .setLayoutCount = 3,
            .pSetLayouts = layouts,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range
        };

        VK_CHECK(qvkCreatePipelineLayout(vk.device, &desc, NULL, &vk.pipeline_layout));
        SET_OBJECT_NAME(vk.pipeline_layout, "main pipeline layout", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT);
    }

    // Storage pipeline layout (for compute)
    {
        VkDescriptorSetLayout layouts[1] = { vk.compute_descriptor_set_layout };

        VkPipelineLayoutCreateInfo desc = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .setLayoutCount = 1,
            .pSetLayouts = layouts,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = NULL
        };

        VK_CHECK(qvkCreatePipelineLayout(vk.device, &desc, NULL, &vk.pipeline_layout_storage));
        SET_OBJECT_NAME(vk.pipeline_layout_storage, "storage pipeline layout", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT);
    }
}

// Unified descriptor write function from vk.c is used

// Update uniform descriptor
extern "C" void vk_update_uniform_descriptor(VkDescriptorSet descriptor, VkBuffer buffer)
{
    VkDescriptorBufferInfo info[2];
    VkWriteDescriptorSet desc[2];

    vk_write_uniform_descriptor(desc, info, buffer, descriptor, VK_DESC_UNIFORM_MAIN_BINDING, sizeof(vkUniform_t));
    vk_write_uniform_descriptor(desc, info, buffer, descriptor, VK_DESC_UNIFORM_CAMERA_BINDING, sizeof(vkUniformCamera_t));

    qvkUpdateDescriptorSets(vk.device, 2, desc, 0, NULL);
}

// Update attachment descriptors
extern "C" void vk_update_attachment_descriptors(void) {
    if (!vk.color_image_view) {
        return;
    }

    VkDescriptorImageInfo info;
    VkWriteDescriptorSet desc;
    Vk_Sampler_Def sd;
    uint32_t i;

    Com_Memset(&sd, 0, sizeof(sd));
    // Always use linear filtering for color buffer when used for bloom/post-processing
    // to avoid blocky artifacts. vk.blitFilter (which can be VK_FILTER_NEAREST) is only for final blit to screen.
    sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
    sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sd.max_lod_1_0 = qtrue;
    sd.noAnisotropy = qtrue;

    info.sampler = vk_find_sampler(&sd);
    info.imageView = vk.color_image_view;
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    desc.dstSet = vk.color_descriptor;
    desc.dstBinding = 0;
    desc.dstArrayElement = 0;
    desc.descriptorCount = 1;
    desc.pNext = NULL;
    desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc.pImageInfo = &info;
    desc.pBufferInfo = NULL;
    desc.pTexelBufferView = NULL;

    qvkUpdateDescriptorSets(vk.device, 1, &desc, 0, NULL);

    // screenmap
    info.imageView = vk.screenMap.color_image_view;
    desc.dstSet = vk.screenMap.color_descriptor;
    qvkUpdateDescriptorSets(vk.device, 1, &desc, 0, NULL);

    if (r_bloom && r_bloom->integer) {
        Com_Memset(&sd, 0, sizeof(sd));
        sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
        sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sd.max_lod_1_0 = qtrue;
        sd.noAnisotropy = qtrue;
        info.sampler = vk_find_sampler(&sd);

        for (i = 0; i < ARRAY_LEN(vk.bloom_image_descriptor); i++) {
            info.imageView = vk.bloom_image_view[i];
            desc.dstSet = vk.bloom_image_descriptor[i];
            qvkUpdateDescriptorSets(vk.device, 1, &desc, 0, NULL);
        }
    }

#ifdef VK_PBR_BRDFLUT
  // BRDF LUT: update descriptor with BRDF LUT image+sampler
  if (vk.brdflut.view != VK_NULL_HANDLE && vk.brdflut_image_descriptor != VK_NULL_HANDLE) {
    VkDescriptorImageInfo brdfInfo = {};
    brdfInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    brdfInfo.imageView = vk.brdflut.view;
    ensure_brdf_sampler();
    brdfInfo.sampler = brdflut_sampler;

    VkWriteDescriptorSet brdfDesc = {};
    brdfDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    brdfDesc.dstSet = vk.brdflut_image_descriptor;
    brdfDesc.dstBinding = 0;
    brdfDesc.dstArrayElement = 0;
    brdfDesc.descriptorCount = 1;
    brdfDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    brdfDesc.pImageInfo = &brdfInfo;
    brdfDesc.pBufferInfo = NULL;
    brdfDesc.pTexelBufferView = NULL;
    qvkUpdateDescriptorSets(vk.device, 1, &brdfDesc, 0, NULL);
  }
#endif

// cubemap binding disabled due to C++ Vulkan handle type issues
#if 0
if (vk.cubeMap.color_image_view[0] != (void*)0) {
    info.imageView = (VkImageView)vk.cubeMap.color_image_view[0];
    desc.dstSet = vk.cubeMap.color_descriptor;
    qvkUpdateDescriptorSets(vk.device, 1, &desc, 0, NULL);
}
#endif
}

// Initialize descriptors
extern "C" void vk_init_descriptors(void) {
    VkDescriptorSetAllocateInfo alloc;
    VkDescriptorBufferInfo info;
    VkWriteDescriptorSet desc;
    uint32_t i;

    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.pNext = NULL;
    alloc.descriptorPool = vk.descriptor_pool;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &vk.set_layout_storage;

    // Validate before allocation
    if (vk.descriptor_pool == VK_NULL_HANDLE) {
        ri.Printf(PRINT_ERROR, "vk_init_descriptors: descriptor_pool is NULL!\n");
        return;
    }
    if (vk.set_layout_storage == VK_NULL_HANDLE) {
        ri.Printf(PRINT_ERROR, "vk_init_descriptors: set_layout_storage is NULL!\n");
        return;
    }
    if (qvkAllocateDescriptorSets == NULL) {
        ri.Printf(PRINT_ERROR, "vk_init_descriptors: qvkAllocateDescriptorSets function pointer is NULL!\n");
        return;
    }

    VkResult result = qvkAllocateDescriptorSets(vk.device, &alloc, &vk.storage.descriptor);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_init_descriptors: qvkAllocateDescriptorSets failed: %s\n", vk_result_string(result));
        return;
    }

    // Allocate ray tracing descriptor set if supported
    if (vk.rayTracingSupported && vk.rt.initialized && vk.rt.raytracingDescriptorSetLayout != VK_NULL_HANDLE) {
        alloc.pSetLayouts = &vk.rt.raytracingDescriptorSetLayout;
        VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.rt.raytracingDescriptorSet));
        SET_OBJECT_NAME(vk.rt.raytracingDescriptorSet, "ray tracing descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
    }

    info.buffer = vk.storage.buffer;
    info.offset = 0;
    info.range = sizeof(uint32_t);

    desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    desc.dstSet = vk.storage.descriptor;
    desc.dstBinding = 0;
    desc.dstArrayElement = 0;
    desc.descriptorCount = 1;
    desc.pNext = NULL;
    desc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    desc.pImageInfo = NULL;
    desc.pBufferInfo = &info;
    desc.pTexelBufferView = NULL;

    qvkUpdateDescriptorSets(vk.device, 1, &desc, 0, NULL);

#ifdef VK_PBR_BRDFLUT
    // Allocate BRDF LUT descriptor set (uses the same sampler layout as color sampler)
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &vk.set_layout_sampler;
    VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.brdflut_image_descriptor));
    SET_OBJECT_NAME(vk.brdflut_image_descriptor, "BRDF LUT image descriptor", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
#endif

    // Allocate and update descriptor set for each command buffer
    for (i = 0; i < NUM_COMMAND_BUFFERS; i++) {
        alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.pNext = NULL;
        alloc.descriptorPool = vk.descriptor_pool;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts = &vk.set_layout_uniform;

        VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.tess[i].uniform_descriptor));

        vk_update_uniform_descriptor(vk.tess[i].uniform_descriptor, vk.tess[i].vertex_buffer);

        SET_OBJECT_NAME(vk.tess[i].uniform_descriptor, va("uniform descriptor %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
    }

    if (vk.color_image_view) {
        alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.pNext = NULL;
        alloc.descriptorPool = vk.descriptor_pool;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts = &vk.set_layout_sampler;

        VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.color_descriptor));

        // Allocate compute descriptor set for post-processing
        if (vk.compute_descriptor_set_layout != VK_NULL_HANDLE) {
            alloc.pSetLayouts = &vk.compute_descriptor_set_layout;
            VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.compute_descriptor_set));
            SET_OBJECT_NAME(vk.compute_descriptor_set, "compute descriptor set - post-processing", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
        }

        if (r_bloom && r_bloom->integer) {
            for (i = 0; i < ARRAY_LEN(vk.bloom_image_descriptor); i++) {
                VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.bloom_image_descriptor[i]));
            }
        }

        // Allocate enhanced post-processing descriptor sets
        if (vk.ssao_descriptor_layout != VK_NULL_HANDLE) {
            alloc.pSetLayouts = &vk.ssao_descriptor_layout;
            VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.ssao_descriptor));
            SET_OBJECT_NAME(vk.ssao_descriptor, "SSAO descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
        }
        if (vk.ssr_descriptor_layout != VK_NULL_HANDLE) {
            alloc.pSetLayouts = &vk.ssr_descriptor_layout;
            VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.ssr_descriptor));
            SET_OBJECT_NAME(vk.ssr_descriptor, "SSR descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
        }
        if (vk.bloom_descriptor_layout != VK_NULL_HANDLE) {
            alloc.pSetLayouts = &vk.bloom_descriptor_layout;
            VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.bloom_descriptor));
            SET_OBJECT_NAME(vk.bloom_descriptor, "Bloom descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
        }
        if (vk.dof_descriptor_layout != VK_NULL_HANDLE) {
            alloc.pSetLayouts = &vk.dof_descriptor_layout;
            VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.dof_descriptor));
            SET_OBJECT_NAME(vk.dof_descriptor, "DoF descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
        }
        if (vk.velocity_tiles_descriptor_layout != VK_NULL_HANDLE) {
            alloc.pSetLayouts = &vk.velocity_tiles_descriptor_layout;
            VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.velocity_tiles_descriptor));
            SET_OBJECT_NAME(vk.velocity_tiles_descriptor, "Velocity tiles descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
        }
        if (vk.motion_blur_descriptor_layout != VK_NULL_HANDLE) {
            alloc.pSetLayouts = &vk.motion_blur_descriptor_layout;
            VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.motion_blur_descriptor));
            SET_OBJECT_NAME(vk.motion_blur_descriptor, "Motion blur descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
        }
        if (vk.color_grading_descriptor_layout != VK_NULL_HANDLE) {
            alloc.pSetLayouts = &vk.color_grading_descriptor_layout;
            VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.color_grading_descriptor));
            SET_OBJECT_NAME(vk.color_grading_descriptor, "Color grading descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
        }
        if (vk.heat_distortion_descriptor_layout != VK_NULL_HANDLE) {
            alloc.pSetLayouts = &vk.heat_distortion_descriptor_layout;
            VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.heat_distortion_descriptor));
            SET_OBJECT_NAME(vk.heat_distortion_descriptor, "Heat distortion descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
        }

        alloc.descriptorSetCount = 1;
        VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.screenMap.color_descriptor));

        // cubemap
        VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc, &vk.cubeMap.color_descriptor));

        vk_update_attachment_descriptors();
    }
}
