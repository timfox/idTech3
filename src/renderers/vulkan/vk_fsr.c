#include "vk_fsr.h"
#include "vk.h"
#include "tr_local.h"
#include "qvk.h"

// CVARs
cvar_t *r_fsr_enable = NULL;
cvar_t *r_fsr_easu = NULL;
cvar_t *r_fsr_rcas = NULL;
cvar_t *r_fsr_sharpness = NULL;

// Global FSR state
static vk_fsr_state_t vk_fsr_state = {0};

// Forward declarations for shader data
extern unsigned char fsr_easu_comp_spv[];
extern unsigned int fsr_easu_comp_spv_size;
extern unsigned char fsr_rcas_comp_spv[];
extern unsigned int fsr_rcas_comp_spv_size;

// Initialize FSR CVARs
static void vk_fsr_init_cvars(void) {
    r_fsr_enable = ri.Cvar_Get("r_fsr_enable", "0", CVAR_ARCHIVE);
    r_fsr_easu = ri.Cvar_Get("r_fsr_easu", "1", CVAR_ARCHIVE);
    r_fsr_rcas = ri.Cvar_Get("r_fsr_rcas", "1", CVAR_ARCHIVE);
    r_fsr_sharpness = ri.Cvar_Get("r_fsr_sharpness", "0.2", CVAR_ARCHIVE);
}

// Create descriptor set layout for FSR
static qboolean vk_fsr_create_descriptor_layout(void) {
    VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        }
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = LENGTH(bindings),
        .pBindings = bindings,
    };

    VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &layout_info, NULL,
                                         &vk_fsr_state.descriptor_layout));

    return qtrue;
}

// Create pipeline layout for FSR
static qboolean vk_fsr_create_pipeline_layout(void) {
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &vk_fsr_state.descriptor_layout,
    };

    VK_CHECK(qvkCreatePipelineLayout(vk.device, &pipeline_layout_info, NULL,
                                   &vk_fsr_state.pipeline_layout));

    return qtrue;
}

// Create descriptor pool and set for FSR
static qboolean vk_fsr_create_descriptor_pool(void) {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = LENGTH(pool_sizes),
        .pPoolSizes = pool_sizes
    };

    VK_CHECK(qvkCreateDescriptorPool(vk.device, &pool_info, NULL, &vk_fsr_state.descriptor_pool));

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vk_fsr_state.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &vk_fsr_state.descriptor_layout
    };

    VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc_info, &vk_fsr_state.descriptor_set));

    return qtrue;
}

// Create constants buffer for FSR
static qboolean vk_fsr_create_constants_buffer(void) {
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(vk_fsr_easu_constants_t) + sizeof(vk_fsr_rcas_constants_t),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VK_CHECK(qvkCreateBuffer(vk.device, &buffer_info, NULL, &vk_fsr_state.constants_buffer));

    VkMemoryRequirements mem_reqs;
    qvkGetBufferMemoryRequirements(vk.device, vk_fsr_state.constants_buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };

    VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk_fsr_state.constants_memory));
    VK_CHECK(qvkBindBufferMemory(vk.device, vk_fsr_state.constants_buffer,
                               vk_fsr_state.constants_memory, 0));

    VK_CHECK(qvkMapMemory(vk.device, vk_fsr_state.constants_memory, 0,
                        buffer_info.size, 0, &vk_fsr_state.constants_mapped));

    return qtrue;
}

// Create compute pipelines for FSR
qboolean vk_fsr_create_pipelines(void) {
    if (!vk_fsr_create_descriptor_layout()) {
        return qfalse;
    }

    if (!vk_fsr_create_pipeline_layout()) {
        return qfalse;
    }

    if (!vk_fsr_create_descriptor_pool()) {
        return qfalse;
    }

    if (!vk_fsr_create_constants_buffer()) {
        return qfalse;
    }

    // Create shader modules
    VkShaderModuleCreateInfo easu_module_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fsr_easu_comp_spv_size,
        .pCode = (uint32_t*)fsr_easu_comp_spv,
    };

    VkShaderModuleCreateInfo rcas_module_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fsr_rcas_comp_spv_size,
        .pCode = (uint32_t*)fsr_rcas_comp_spv,
    };

    VkShaderModule easu_module, rcas_module;
    VK_CHECK(qvkCreateShaderModule(vk.device, &easu_module_info, NULL, &easu_module));
    VK_CHECK(qvkCreateShaderModule(vk.device, &rcas_module_info, NULL, &rcas_module));

    // Create pipelines
    VkComputePipelineCreateInfo pipeline_infos[FSR_NUM_PIPELINES] = {
        [FSR_EASU_TO_RCAS] = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = easu_module,
                .pName = "main",
            },
            .layout = vk_fsr_state.pipeline_layout,
        },
        [FSR_EASU_TO_DISPLAY] = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = easu_module,
                .pName = "main",
            },
            .layout = vk_fsr_state.pipeline_layout,
        },
        [FSR_RCAS_AFTER_EASU] = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = rcas_module,
                .pName = "main",
            },
            .layout = vk_fsr_state.pipeline_layout,
        },
        [FSR_RCAS_AFTER_TAAU] = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = rcas_module,
                .pName = "main",
            },
            .layout = vk_fsr_state.pipeline_layout,
        },
    };

    VK_CHECK(qvkCreateComputePipelines(vk.device, VK_NULL_HANDLE, FSR_NUM_PIPELINES,
                                     pipeline_infos, NULL, vk_fsr_state.pipelines));

    // Cleanup shader modules
    qvkDestroyShaderModule(vk.device, easu_module, NULL);
    qvkDestroyShaderModule(vk.device, rcas_module, NULL);

    return qtrue;
}

// Initialize FSR system
qboolean vk_fsr_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing FSR (FidelityFX Super Resolution)\n");

    vk_fsr_init_cvars();

    if (!vk_fsr_create_pipelines()) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create FSR pipelines\n");
        return qfalse;
    }

    vk_fsr_state.initialized = qtrue;
    ri.Printf(PRINT_ALL, "Vulkan: FSR initialized successfully\n");

    return qtrue;
}

// Shutdown FSR system
void vk_fsr_shutdown(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Shutting down FSR\n");

    if (vk_fsr_state.constants_mapped) {
        qvkUnmapMemory(vk.device, vk_fsr_state.constants_memory);
        vk_fsr_state.constants_mapped = NULL;
    }

    if (vk_fsr_state.constants_buffer != VK_NULL_HANDLE) {
        qvkDestroyBuffer(vk.device, vk_fsr_state.constants_buffer, NULL);
        vk_fsr_state.constants_buffer = VK_NULL_HANDLE;
    }

    if (vk_fsr_state.constants_memory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk_fsr_state.constants_memory, NULL);
        vk_fsr_state.constants_memory = VK_NULL_HANDLE;
    }

    vk_fsr_destroy_pipelines();

    if (vk_fsr_state.descriptor_pool != VK_NULL_HANDLE) {
        qvkDestroyDescriptorPool(vk.device, vk_fsr_state.descriptor_pool, NULL);
        vk_fsr_state.descriptor_pool = VK_NULL_HANDLE;
    }

    if (vk_fsr_state.pipeline_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk_fsr_state.pipeline_layout, NULL);
        vk_fsr_state.pipeline_layout = VK_NULL_HANDLE;
    }

    if (vk_fsr_state.descriptor_layout != VK_NULL_HANDLE) {
        qvkDestroyDescriptorSetLayout(vk.device, vk_fsr_state.descriptor_layout, NULL);
        vk_fsr_state.descriptor_layout = VK_NULL_HANDLE;
    }

    vk_fsr_state.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: FSR shut down\n");
}

// Destroy FSR pipelines
void vk_fsr_destroy_pipelines(void) {
    for (int i = 0; i < FSR_NUM_PIPELINES; i++) {
        if (vk_fsr_state.pipelines[i] != VK_NULL_HANDLE) {
            qvkDestroyPipeline(vk.device, vk_fsr_state.pipelines[i], NULL);
            vk_fsr_state.pipelines[i] = VK_NULL_HANDLE;
        }
    }
}

// Check if FSR is enabled
qboolean vk_fsr_is_enabled(void) {
    if (!vk_fsr_state.initialized || r_fsr_enable->integer == 0) {
        return qfalse;
    }

    // Only apply when upscaling
    if (vk.extent_render.width >= vk.extent_unscaled.width ||
        vk.extent_render.height >= vk.extent_unscaled.height) {
        return qfalse;
    }

    return (r_fsr_easu->integer != 0) || (r_fsr_rcas->integer != 0);
}

// Update FSR constants
void vk_fsr_update_constants(uint32_t render_width, uint32_t render_height,
                           uint32_t display_width, uint32_t display_height) {
    if (!vk_fsr_state.initialized || !vk_fsr_state.constants_mapped) {
        return;
    }

    // Set EASU constants
    vk_fsr_easu_constants_t* easu_consts = (vk_fsr_easu_constants_t*)vk_fsr_state.constants_mapped;
    FsrEasuCon(easu_consts->easu_const0, easu_consts->easu_const1,
               easu_consts->easu_const2, easu_consts->easu_const3,
               render_width, render_height,
               render_width, render_height,
               display_width, display_height);

    // Set RCAS constants
    vk_fsr_rcas_constants_t* rcas_consts = (vk_fsr_rcas_constants_t*)
        ((char*)vk_fsr_state.constants_mapped + sizeof(vk_fsr_easu_constants_t));
    FsrRcasCon(rcas_consts->rcas_const0, r_fsr_sharpness->value);
}

// Apply EASU upscaling
void vk_fsr_apply_easu(VkCommandBuffer cmd_buf, VkImage input_image,
                      VkImageView input_view, VkImage output_image, VkImageView output_view) {
    if (!vk_fsr_is_enabled() || !r_fsr_easu->integer) {
        return;
    }

    // Update descriptor set for EASU pass
    VkDescriptorImageInfo input_info = {
        .sampler = VK_NULL_HANDLE,
        .imageView = input_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo output_info = {
        .sampler = VK_NULL_HANDLE,
        .imageView = output_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorBufferInfo buffer_info = {
        .buffer = vk_fsr_state.constants_buffer,
        .offset = 0,
        .range = sizeof(vk_fsr_easu_constants_t)
    };

    VkWriteDescriptorSet writes[3] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vk_fsr_state.descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = &input_info
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vk_fsr_state.descriptor_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &output_info
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vk_fsr_state.descriptor_set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buffer_info
        }
    };

    qvkUpdateDescriptorSets(vk.device, 3, writes, 0, NULL);

    // Bind pipeline and descriptor set
    VkPipeline easu_pipeline = qvk.surf_is_hdr ?
        vk_fsr_state.pipelines[FSR_EASU_TO_RCAS] : vk_fsr_state.pipelines[FSR_EASU_TO_DISPLAY];
    qvkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, easu_pipeline);
    qvkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
                           vk_fsr_state.pipeline_layout, 0, 1, &vk_fsr_state.descriptor_set, 0, NULL);

    // Calculate dispatch size
    uint32_t dispatch_x = (vk.extent_unscaled.width + 15) / 16;
    uint32_t dispatch_y = (vk.extent_unscaled.height + 15) / 16;

    qvkCmdDispatch(cmd_buf, dispatch_x, dispatch_y, 1);
}

// Apply RCAS sharpening
void vk_fsr_apply_rcas(VkCommandBuffer cmd_buf, VkImage input_image,
                      VkImageView input_view, VkImage output_image, VkImageView output_view) {
    if (!vk_fsr_is_enabled() || !r_fsr_rcas->integer) {
        return;
    }

    // Update descriptor set for RCAS pass
    VkDescriptorImageInfo input_info = {
        .sampler = VK_NULL_HANDLE,
        .imageView = input_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo output_info = {
        .sampler = VK_NULL_HANDLE,
        .imageView = output_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorBufferInfo buffer_info = {
        .buffer = vk_fsr_state.constants_buffer,
        .offset = sizeof(vk_fsr_easu_constants_t),
        .range = sizeof(vk_fsr_rcas_constants_t)
    };

    VkWriteDescriptorSet writes[3] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vk_fsr_state.descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = &input_info
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vk_fsr_state.descriptor_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &output_info
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vk_fsr_state.descriptor_set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buffer_info
        }
    };

    qvkUpdateDescriptorSets(vk.device, 3, writes, 0, NULL);

    // Bind pipeline and descriptor set
    VkPipeline rcas_pipeline = qvk.surf_is_hdr ?
        vk_fsr_state.pipelines[FSR_RCAS_AFTER_EASU] : vk_fsr_state.pipelines[FSR_RCAS_AFTER_TAAU];
    qvkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, rcas_pipeline);
    qvkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
                           vk_fsr_state.pipeline_layout, 0, 1, &vk_fsr_state.descriptor_set, 0, NULL);

    // Calculate dispatch size
    uint32_t dispatch_x = (vk.extent_unscaled.width + 15) / 16;
    uint32_t dispatch_y = (vk.extent_unscaled.height + 15) / 16;

    qvkCmdDispatch(cmd_buf, dispatch_x, dispatch_y, 1);
}