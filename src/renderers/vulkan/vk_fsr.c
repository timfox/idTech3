#include "vk_fsr.h"
#include "tr_local.h"
#include "vk.h"

#define A_CPU
#include "fsr/ffx_a.h"
#include "fsr/ffx_fsr1.h"

extern refimport_t ri;
extern cvar_t *r_hdr;
extern glconfig_t glConfig;

// Global FSR state
static vk_fsr_state_t vk_fsr_state = {0};

// Forward declarations for shader data from shader_data.c
extern const unsigned char fsr_easu_comp_spv[];
extern const unsigned int fsr_easu_comp_spv_size;
extern const unsigned char fsr_rcas_comp_spv[];
extern const unsigned int fsr_rcas_comp_spv_size;

void vk_fsr_destroy_pipelines(void) {
    for (int i = 0; i < FSR_NUM_PIPELINES; i++) {
        if (vk_fsr_state.pipelines[i]) {
            qvkDestroyPipeline(vk.device, vk_fsr_state.pipelines[i], NULL);
            vk_fsr_state.pipelines[i] = VK_NULL_HANDLE;
        }
    }
}

static qboolean vk_fsr_create_pipeline_layout(void) {
    VkDescriptorSetLayoutBinding bindings[3] = {0};
    
    // Input image (combined sampler)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Output image (storage)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Constants buffer
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info = {0};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 3;
    layout_info.pBindings = bindings;

    VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &layout_info, NULL, &vk_fsr_state.descriptor_layout));

    VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &vk_fsr_state.descriptor_layout;

    VK_CHECK(qvkCreatePipelineLayout(vk.device, &pipeline_layout_info, NULL, &vk_fsr_state.pipeline_layout));

    return qtrue;
}

static qboolean vk_fsr_create_descriptor_pool(void) {
    VkDescriptorPoolSize pool_sizes[3] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 }
    };

    VkDescriptorPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 2; // One for EASU, one for RCAS
    pool_info.poolSizeCount = 3;
    pool_info.pPoolSizes = pool_sizes;

    VK_CHECK(qvkCreateDescriptorPool(vk.device, &pool_info, NULL, &vk_fsr_state.descriptor_pool));

    VkDescriptorSetAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = vk_fsr_state.descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &vk_fsr_state.descriptor_layout;

    VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc_info, &vk_fsr_state.descriptor_sets[0]));
    VK_CHECK(qvkAllocateDescriptorSets(vk.device, &alloc_info, &vk_fsr_state.descriptor_sets[1]));

    return qtrue;
}

static qboolean vk_fsr_create_constants_buffer(void) {
    VkBufferCreateInfo buffer_info = {0};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = sizeof(vk_fsr_easu_constants_t) + sizeof(vk_fsr_rcas_constants_t);
    buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(qvkCreateBuffer(vk.device, &buffer_info, NULL, &vk_fsr_state.constants_buffer));

    VkMemoryRequirements mem_reqs;
    qvkGetBufferMemoryRequirements(vk.device, vk_fsr_state.constants_buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk_fsr_state.constants_memory));
    VK_CHECK(qvkBindBufferMemory(vk.device, vk_fsr_state.constants_buffer, vk_fsr_state.constants_memory, 0));
    VK_CHECK(qvkMapMemory(vk.device, vk_fsr_state.constants_memory, 0, buffer_info.size, 0, &vk_fsr_state.constants_mapped));

    return qtrue;
}

qboolean vk_fsr_create_pipelines(void) {
    VkShaderModule easu_module = vk_create_shader_module(fsr_easu_comp_spv, fsr_easu_comp_spv_size);
    VkShaderModule rcas_module = vk_create_shader_module(fsr_rcas_comp_spv, fsr_rcas_comp_spv_size);

    if (easu_module == VK_NULL_HANDLE || rcas_module == VK_NULL_HANDLE) {
        return qfalse;
    }

    VkComputePipelineCreateInfo pipeline_infos[FSR_NUM_PIPELINES] = {0};
    
    for (int i = 0; i < 2; i++) {
        pipeline_infos[i].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_infos[i].stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_infos[i].stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_infos[i].stage.module = easu_module;
        pipeline_infos[i].stage.pName = "main";
        pipeline_infos[i].layout = vk_fsr_state.pipeline_layout;
    }

    for (int i = 2; i < 4; i++) {
        pipeline_infos[i].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_infos[i].stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_infos[i].stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_infos[i].stage.module = rcas_module;
        pipeline_infos[i].stage.pName = "main";
        pipeline_infos[i].layout = vk_fsr_state.pipeline_layout;
    }

    VK_CHECK(qvkCreateComputePipelines(vk.device, VK_NULL_HANDLE, FSR_NUM_PIPELINES,
                                     pipeline_infos, NULL, vk_fsr_state.pipelines));

    qvkDestroyShaderModule(vk.device, easu_module, NULL);
    qvkDestroyShaderModule(vk.device, rcas_module, NULL);

    return qtrue;
}

qboolean vk_fsr_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing FSR\n");

    r_fsr_enable = ri.Cvar_Get("r_fsr_enable", "0", CVAR_ARCHIVE);
    r_fsr_easu = ri.Cvar_Get("r_fsr_easu", "1", CVAR_ARCHIVE);
    r_fsr_rcas = ri.Cvar_Get("r_fsr_rcas", "1", CVAR_ARCHIVE);
    r_fsr_sharpness = ri.Cvar_Get("r_fsr_sharpness", "0.5", CVAR_ARCHIVE);

    if (!vk_fsr_create_pipeline_layout()) return qfalse;
    if (!vk_fsr_create_descriptor_pool()) return qfalse;
    if (!vk_fsr_create_constants_buffer()) return qfalse;
    if (!vk_fsr_create_pipelines()) return qfalse;

    vk_fsr_state.initialized = qtrue;
    return qtrue;
}

void vk_fsr_shutdown(void) {
    if (!vk_fsr_state.initialized) return;

    vk_fsr_destroy_pipelines();

    if (vk_fsr_state.constants_mapped) {
        qvkUnmapMemory(vk.device, vk_fsr_state.constants_memory);
    }
    if (vk_fsr_state.constants_buffer) {
        qvkDestroyBuffer(vk.device, vk_fsr_state.constants_buffer, NULL);
    }
    if (vk_fsr_state.constants_memory) {
        qvkFreeMemory(vk.device, vk_fsr_state.constants_memory, NULL);
    }
    if (vk_fsr_state.descriptor_pool) {
        qvkDestroyDescriptorPool(vk.device, vk_fsr_state.descriptor_pool, NULL);
    }
    if (vk_fsr_state.descriptor_layout) {
        qvkDestroyDescriptorSetLayout(vk.device, vk_fsr_state.descriptor_layout, NULL);
    }
    if (vk_fsr_state.pipeline_layout) {
        qvkDestroyPipelineLayout(vk.device, vk_fsr_state.pipeline_layout, NULL);
    }

    memset(&vk_fsr_state, 0, sizeof(vk_fsr_state));
}

qboolean vk_fsr_is_enabled(void) {
    // Temporarily disable FSR to avoid unstable state during UI-only rendering.
    return qfalse;
}

void vk_fsr_update_constants(uint32_t render_width, uint32_t render_height,
                           uint32_t display_width, uint32_t display_height) {
    if (!vk_fsr_state.initialized) return;

    vk_fsr_easu_constants_t easu_con;
    FsrEasuCon(easu_con.easu_const0, easu_con.easu_const1, easu_con.easu_const2, easu_con.easu_const3,
               (AF1)render_width, (AF1)render_height, 
               (AF1)render_width, (AF1)render_height,
               (AF1)display_width, (AF1)display_height);

    vk_fsr_rcas_constants_t rcas_con;
    FsrRcasCon(rcas_con.rcas_const0, r_fsr_sharpness->value);

    memcpy(vk_fsr_state.constants_mapped, &easu_con, sizeof(easu_con));
    memcpy((uint8_t*)vk_fsr_state.constants_mapped + sizeof(easu_con), &rcas_con, sizeof(rcas_con));
}

static void vk_fsr_update_descriptor_set(VkDescriptorSet set, VkImageView input_view, VkImageView output_view, VkDeviceSize buffer_offset, VkDeviceSize buffer_range) {
    Vk_Sampler_Def sampler_def = {0};
    sampler_def.vk_min_filter = VK_FILTER_LINEAR;
    sampler_def.vk_mag_filter = VK_FILTER_LINEAR;
    sampler_def.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSampler sampler = vk_find_sampler(&sampler_def);

    VkDescriptorImageInfo input_info = {0};
    input_info.sampler = sampler;
    input_info.imageView = input_view;
    input_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo output_info = {0};
    output_info.imageView = output_view;
    output_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorBufferInfo buffer_info = {0};
    buffer_info.buffer = vk_fsr_state.constants_buffer;
    buffer_info.offset = buffer_offset;
    buffer_info.range = buffer_range;

    VkWriteDescriptorSet writes[3] = {0};
    
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &input_info;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &output_info;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = set;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].pBufferInfo = &buffer_info;

    qvkUpdateDescriptorSets(vk.device, 3, writes, 0, NULL);
}

void vk_fsr_apply_easu(VkCommandBuffer cmd_buf, VkImage input_image,
                      VkImageView input_view, VkImage output_image, VkImageView output_view) {
    (void)input_image; (void)output_image;
    if (!vk_fsr_is_enabled() || !r_fsr_easu->integer) return;

    vk_fsr_update_descriptor_set(vk_fsr_state.descriptor_sets[0], input_view, output_view, 0, sizeof(vk_fsr_easu_constants_t));

    VkPipeline easu_pipeline = (r_hdr && r_hdr->integer) ?
        vk_fsr_state.pipelines[FSR_EASU_TO_RCAS] : vk_fsr_state.pipelines[FSR_EASU_TO_DISPLAY];

    qvkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, easu_pipeline);
    qvkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, vk_fsr_state.pipeline_layout, 0, 1, &vk_fsr_state.descriptor_sets[0], 0, NULL);

    uint32_t dispatch_x = (glConfig.vidWidth + 7) / 8;
    uint32_t dispatch_y = (glConfig.vidHeight + 7) / 8;
    qvkCmdDispatch(cmd_buf, dispatch_x, dispatch_y, 1);
}

void vk_fsr_apply_rcas(VkCommandBuffer cmd_buf, VkImage input_image,
                      VkImageView input_view, VkImage output_image, VkImageView output_view) {
    (void)input_image; (void)output_image;
    if (!vk_fsr_is_enabled() || !r_fsr_rcas->integer) return;

    vk_fsr_update_descriptor_set(vk_fsr_state.descriptor_sets[1], input_view, output_view, sizeof(vk_fsr_easu_constants_t), sizeof(vk_fsr_rcas_constants_t));

    VkPipeline rcas_pipeline = (r_hdr && r_hdr->integer) ?
        vk_fsr_state.pipelines[FSR_RCAS_AFTER_EASU] : vk_fsr_state.pipelines[FSR_RCAS_AFTER_TAAU];

    qvkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, rcas_pipeline);
    qvkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, vk_fsr_state.pipeline_layout, 0, 1, &vk_fsr_state.descriptor_sets[1], 0, NULL);

    uint32_t dispatch_x_rcas = (glConfig.vidWidth + 7) / 8;
    uint32_t dispatch_y_rcas = (glConfig.vidHeight + 7) / 8;
    qvkCmdDispatch(cmd_buf, dispatch_x_rcas, dispatch_y_rcas, 1);
}
