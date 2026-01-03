/*
=============================================================================
Vulkan Shader Management - C++23 Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include <vector>
#include <cstring>
#include <unordered_map>
#include <string>

#ifdef USE_VULKAN

// External Vulkan objects
extern VkInstance vk_instance;
extern VkPhysicalDevice vk_physical_device;
extern VkDevice vk_device;
extern Vk_Instance vk;

// Vulkan function pointers
extern PFN_vkCreateShaderModule qvkCreateShaderModule;
extern PFN_vkDestroyShaderModule qvkDestroyShaderModule;
extern PFN_vkCreateGraphicsPipelines qvkCreateGraphicsPipelines;

// Embedded SPIR-V shader extern declarations (from shader_data.c)
extern const unsigned char dot_vert_spv[1192];
extern const unsigned char dot_frag_spv[544];
extern const unsigned char color_vert_spv[872];
extern const unsigned char color_frag_spv[1296];
extern const unsigned char fog_vert_spv[2700];
extern const unsigned char fog_frag_spv[1240];

// Shader cache structure
struct VkShaderCache {
    std::unordered_map<std::string, VkShaderModule> vertexShaders;
    std::unordered_map<std::string, VkShaderModule> fragmentShaders;
    std::unordered_map<std::string, VkShaderModule> computeShaders;
};

static VkShaderCache shader_cache;

namespace shader_mgr {

// Shader type enumeration
enum class ShaderType {
    VERTEX,
    FRAGMENT,
    COMPUTE,
    GEOMETRY,
    TESSELLATION_CONTROL,
    TESSELLATION_EVALUATION
};

// Load shader from embedded SPIR-V data (embedded within the binary)
// Note: This provides a minimal set of embedded shaders by mapping known
// shader names to prebuilt SPIR-V blobs defined in shader_data.c.
VkShaderModule load_embedded_shader(const char* shader_name, ShaderType type) {
    // Lightweight string compare for a few known shaders
    if (shader_name == nullptr) return VK_NULL_HANDLE;
    // Helper macro to create a module from embedded data
    auto make_module = [](const void* data, size_t size) -> VkShaderModule {
        if (!data || size == 0) return VK_NULL_HANDLE;
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = size;
        createInfo.pCode = reinterpret_cast<const uint32_t*>(data);
        VkShaderModule module;
        if (vkCreateShaderModule(vk.device, &createInfo, nullptr, &module) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }
        return module;
    };

    // Dot shaders
    // Support both exact names and common aliases to maximize embed-hit rate
    if ((strcmp(shader_name, "dot_vert") == 0 || strcmp(shader_name, "dot_vert_spv") == 0) && type == ShaderType::VERTEX) {
        return make_module(dot_vert_spv, sizeof(dot_vert_spv));
    }
    if ((strcmp(shader_name, "dot_frag") == 0 || strcmp(shader_name, "dot_frag_spv") == 0) && type == ShaderType::FRAGMENT) {
        return make_module(dot_frag_spv, sizeof(dot_frag_spv));
    }
    // Color shaders
    if ((strcmp(shader_name, "color_vert") == 0 || strcmp(shader_name, "color_vert_spv") == 0) && type == ShaderType::VERTEX) {
        return make_module(color_vert_spv, sizeof(color_vert_spv));
    }
    if ((strcmp(shader_name, "color_frag") == 0 || strcmp(shader_name, "color_frag_spv") == 0) && type == ShaderType::FRAGMENT) {
        return make_module(color_frag_spv, sizeof(color_frag_spv));
    }
    // Fog shaders
    if ((strcmp(shader_name, "fog_vert") == 0 || strcmp(shader_name, "fog_vert_spv") == 0) && type == ShaderType::VERTEX) {
        return make_module(fog_vert_spv, sizeof(fog_vert_spv));
    }
    if ((strcmp(shader_name, "fog_frag") == 0 || strcmp(shader_name, "fog_frag_spv") == 0) && type == ShaderType::FRAGMENT) {
        return make_module(fog_frag_spv, sizeof(fog_frag_spv));
    }

    // If nothing matched, fall back to null
    ri.Printf(PRINT_DEVELOPER, "Shader Manager: No embedded SPIR-V match for %s\n", shader_name);
    return VK_NULL_HANDLE;
}

// Load shader from file (development/debugging)
VkShaderModule load_shader_from_file(const char* filename) {
    // Load SPIR-V file from disk
    void *buffer;
    int file_len = ri.FS_ReadFile(filename, &buffer);

    if (file_len <= 0 || !buffer) {
        // Primary path failed. Attempt a robust fallback to the repository's spirv folder.
        ri.Printf(PRINT_WARNING, "Shader Manager: Could not open shader file %s\n", filename);
        // Build an alternate path using the known repository layout
        std::string fname = filename;
        // Extract basename
        size_t last_slash = fname.find_last_of("/\\\\");
        std::string basename = (last_slash == std::string::npos) ? fname : fname.substr(last_slash + 1);
        std::string alt_path = std::string("/home/tim/Desktop/idtech3/src/renderers/vulkan/shaders/spirv/") + basename;
        // Try alternate path
        const char* alt_cstr = alt_path.c_str();
        int alt_len = ri.FS_ReadFile(alt_cstr, &buffer);
        if (alt_len <= 0 || !buffer) {
            return VK_NULL_HANDLE;
        } else {
            file_len = alt_len;
            // Now proceed with the buffer loaded from alternate path
        }
    }

    std::vector<uint8_t> spirv_data(static_cast<size_t>(file_len));
    memcpy(spirv_data.data(), buffer, static_cast<size_t>(file_len));
    ri.FS_FreeFile(buffer);

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = spirv_data.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(spirv_data.data());

    VkShaderModule shader_module;
    VkResult result = qvkCreateShaderModule(vk.device, &create_info, nullptr, &shader_module);

    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Shader Manager: Failed to create shader module from %s: %s\n",
                 filename, vk_result_string(result));
        return VK_NULL_HANDLE;
    }

    ri.Printf(PRINT_DEVELOPER, "Shader Manager: Loaded shader from file %s\n", filename);
    return shader_module;
}

// Get shader stage flag from type
VkShaderStageFlagBits get_shader_stage(ShaderType type) {
    switch (type) {
        case ShaderType::VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderType::FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderType::COMPUTE: return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderType::GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderType::TESSELLATION_CONTROL: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderType::TESSELLATION_EVALUATION: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        default: return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

} // namespace shader_mgr

// Public interface functions

extern "C" {

// Initialize shader management system
qboolean vk_shader_manager_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing shader management system\n");
    shader_cache.vertexShaders.clear();
    shader_cache.fragmentShaders.clear();
    shader_cache.computeShaders.clear();
    return qtrue;
}

// Shutdown shader management system
void vk_shader_manager_shutdown(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Shutting down shader management system\n");

    // Destroy cached shader modules
    for (auto& pair : shader_cache.vertexShaders) {
        if (pair.second != VK_NULL_HANDLE) {
            qvkDestroyShaderModule(vk.device, pair.second, nullptr);
        }
    }
    shader_cache.vertexShaders.clear();

    for (auto& pair : shader_cache.fragmentShaders) {
        if (pair.second != VK_NULL_HANDLE) {
            qvkDestroyShaderModule(vk.device, pair.second, nullptr);
        }
    }
    shader_cache.fragmentShaders.clear();

    for (auto& pair : shader_cache.computeShaders) {
        if (pair.second != VK_NULL_HANDLE) {
            qvkDestroyShaderModule(vk.device, pair.second, nullptr);
        }
    }
    shader_cache.computeShaders.clear();
}

// Load a shader by name and type
VkShaderModule vk_load_shader(const char* shader_name, VkShaderStageFlagBits stage) {
    std::string key = std::string(shader_name) + "_" + std::to_string(static_cast<int>(stage));

    // Check cache first
    VkShaderModule* cached_module = nullptr;
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            cached_module = &shader_cache.vertexShaders[key];
            break;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            cached_module = &shader_cache.fragmentShaders[key];
            break;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            cached_module = &shader_cache.computeShaders[key];
            break;
        default:
            ri.Printf(PRINT_WARNING, "Shader Manager: Unsupported shader stage for %s\n", shader_name);
            return VK_NULL_HANDLE;
    }

    if (*cached_module != VK_NULL_HANDLE) {
        return *cached_module;
    }

    // Try to load embedded shader first
    shader_mgr::ShaderType type;
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT: type = shader_mgr::ShaderType::VERTEX; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: type = shader_mgr::ShaderType::FRAGMENT; break;
        case VK_SHADER_STAGE_COMPUTE_BIT: type = shader_mgr::ShaderType::COMPUTE; break;
        default: return VK_NULL_HANDLE;
    }

    VkShaderModule module = shader_mgr::load_embedded_shader(shader_name, type);

    // Do not fall back to disk loading; rely solely on embedded SPIR-V.
    if (module != VK_NULL_HANDLE) {
        *cached_module = module;
        ri.Printf(PRINT_DEVELOPER, "Shader Manager: Loaded and cached shader %s\n", shader_name);
    } else {
        ri.Printf(PRINT_WARNING, "Shader Manager: Embedded shader not found for %s; not loading from disk\n", shader_name);
        return VK_NULL_HANDLE;
    }

    if (module != VK_NULL_HANDLE) {
        *cached_module = module;
        ri.Printf(PRINT_DEVELOPER, "Shader Manager: Loaded and cached shader %s\n", shader_name);
    } else {
        ri.Printf(PRINT_WARNING, "Shader Manager: Failed to load shader %s\n", shader_name);
    }

    return module;
}

// Create a basic graphics pipeline with vertex and fragment shaders
qboolean vk_create_basic_pipeline(const char* vertex_shader, const char* fragment_shader,
                                 VkPipelineLayout pipeline_layout, VkRenderPass render_pass,
                                 VkPipeline* pipeline) {
    VkShaderModule vert_module = vk_load_shader(vertex_shader, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderModule frag_module = vk_load_shader(fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT);

    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        ri.Printf(PRINT_ERROR, "Shader Manager: Failed to load shaders for pipeline\n");
        return qfalse;
    }

    VkPipelineShaderStageCreateInfo shader_stages[2] = {};
    shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = vert_module;
    shader_stages[0].pName = "main";

    shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = frag_module;
    shader_stages[1].pName = "main";

    // Basic vertex input state (position, texcoord, color)
    VkVertexInputBindingDescription binding_desc = {};
    binding_desc.binding = 0;
    binding_desc.stride = sizeof(float) * 8; // 2 pos + 2 uv + 4 color
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attr_descs[3] = {};
    // Position
    attr_descs[0].location = 0;
    attr_descs[0].binding = 0;
    attr_descs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attr_descs[0].offset = 0;
    // TexCoord
    attr_descs[1].location = 1;
    attr_descs[1].binding = 0;
    attr_descs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attr_descs[1].offset = sizeof(float) * 2;
    // Color
    attr_descs[2].location = 2;
    attr_descs[2].binding = 0;
    attr_descs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attr_descs[2].offset = sizeof(float) * 4;

    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding_desc;
    vertex_input.vertexAttributeDescriptionCount = 3;
    vertex_input.pVertexAttributeDescriptions = attr_descs;

    // Basic input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Basic viewport state (dynamic)
    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    // Basic rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Basic multisample state
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Basic color blend state
    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blending = {};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    // Dynamic states
    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;

    VkResult result = qvkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pipeline_info,
                                                nullptr, pipeline);

    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Shader Manager: Failed to create graphics pipeline: %s\n",
                 vk_result_string(result));
        return qfalse;
    }

    ri.Printf(PRINT_DEVELOPER, "Shader Manager: Created graphics pipeline with shaders %s/%s\n",
             vertex_shader, fragment_shader);
    return qtrue;
}

} // extern "C"

#endif // USE_VULKAN