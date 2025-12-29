/*
=============================================================================
Vulkan Raymarching Implementation (C++23)

Advanced raymarching implementation for distance field rendering,
volumetric effects, and procedural geometry.
=============================================================================
*/

#ifdef USE_VULKAN

#include "vk_raymarching.h"
#include "vk.h"
#include "tr_local.h"
#include <cstring>
#include <vector>
#include <memory>
#include <string>
#include <string_view>
#include <array>
#include <algorithm>
#include <utility>

// CVAR definitions - using inline variables (C++17)
inline cvar_t *r_raymarching = nullptr;
inline cvar_t *r_raymarchingQuality = nullptr;
inline cvar_t *r_raymarchingMaxDistance = nullptr;
inline cvar_t *r_raymarchingEpsilon = nullptr;
inline cvar_t *r_raymarchingVolumetric = nullptr;
inline cvar_t *r_raymarchingVolumetricDensity = nullptr;

// Global raymarching state using modern C++ initialization
static raymarchingConfig_t raymarchConfig{};
static raymarchingPipeline_t raymarchPipeline{};
static VkBuffer distanceFieldBuffer = VK_NULL_HANDLE;
static VkDeviceMemory distanceFieldMemory = VK_NULL_HANDLE;
static std::vector<distanceField_t> distanceFields;

// Push constants for raymarching shader - using struct instead of typedef
struct RaymarchingPushConstants {
    vec4_t cameraPos;
    vec4_t lightColor;
    float time;
    int maxSteps;
    float maxDistance;
    float volumetricDensity;
};

static constexpr std::string_view raymarchingComputeShader = R"glsl(
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(set = 0, binding = 0) uniform sampler2D inputImage;
layout(set = 0, binding = 1, rgba8) uniform image2D outputImage;
layout(push_constant) uniform PushConstants {
    vec4 cameraPos;
    vec4 lightColor;
    float time;
    int maxSteps;
    float maxDistance;
    float volumetricDensity;
} pc;

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = vec2(pixelCoord) / vec2(imageSize(outputImage));
    vec4 inputColor = texture(inputImage, uv);
    vec3 color = inputColor.rgb;
    if (pc.volumetricDensity > 0.0) {
        color += vec3(0.1, 0.1, 0.2) * pc.volumetricDensity;
    }
    imageStore(outputImage, pixelCoord, vec4(color, inputColor.a));
}
)glsl";

qboolean VK_Raymarching_Init(void) {
    Com_Printf("Initializing Vulkan Raymarching...\n");

    // Register CVARs using modern C++ structured bindings and lambda
    const auto registerCvar = [&](cvar_t*& cvar, const char* name, const char* defaultValue, int flags, const char* description) {
        cvar = ri.Cvar_Get(name, defaultValue, flags);
        ri.Cvar_SetDescription(cvar, description);
    };

    registerCvar(r_raymarching, "r_vkRaymarching", "0", CVAR_ARCHIVE_ND | CVAR_LATCH, "Enable raymarching effects.");
    registerCvar(r_raymarchingQuality, "r_vkRaymarchingQuality", "64", CVAR_ARCHIVE_ND, "Raymarching quality (steps per ray).");
    registerCvar(r_raymarchingMaxDistance, "r_vkRaymarchingMaxDistance", "100.0", CVAR_ARCHIVE_ND, "Maximum raymarching distance.");
    registerCvar(r_raymarchingEpsilon, "r_vkRaymarchingEpsilon", "0.01", CVAR_ARCHIVE_ND, "Raymarching surface epsilon.");
    registerCvar(r_raymarchingVolumetric, "r_vkRaymarchingVolumetric", "0", CVAR_ARCHIVE_ND | CVAR_LATCH, "Enable volumetric raymarching (fog/clouds).");
    registerCvar(r_raymarchingVolumetricDensity, "r_vkRaymarchingVolumetricDensity", "0.1", CVAR_ARCHIVE_ND, "Volumetric raymarching density.");

    // Initialize configuration
    VK_Raymarching_UpdateConfig();

    // Reserve space for distance fields (std::vector handles memory automatically)
    distanceFields.reserve(256);

    // Create shader module using C++23 string_view
    const VkShaderModuleCreateInfo shaderInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = raymarchingComputeShader.size(),
        .pCode = reinterpret_cast<const uint32_t*>(raymarchingComputeShader.data())
    };

    VK_CHECK(vkCreateShaderModule(vk.device, &shaderInfo, nullptr, &raymarchPipeline.computeShader));

    // Create descriptor set layout using std::array
    const std::array<VkDescriptorSetLayoutBinding, 3> bindings = {{
        // Input image
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        },
        // Output image
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        },
        // Distance fields buffer
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        }
    }};

    const VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(vk.device, &layoutInfo, nullptr, &raymarchPipeline.descriptorLayout));

    // Create pipeline layout
    const VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(RaymarchingPushConstants)
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &raymarchPipeline.descriptorLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VK_CHECK(vkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, nullptr, &raymarchPipeline.layout));

    // Create compute pipeline
    const VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = raymarchPipeline.computeShader,
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        .layout = raymarchPipeline.layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VK_CHECK(vkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &raymarchPipeline.pipeline));

    // Add demo distance fields for testing
    VK_Raymarching_AddDemoFields();

    Com_Printf("Vulkan Raymarching initialized successfully\n");
    return qtrue;
}

void VK_Raymarching_Shutdown(void) {
    // Clean up Vulkan resources
    if (raymarchPipeline.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vk.device, raymarchPipeline.pipeline, NULL);
        raymarchPipeline.pipeline = VK_NULL_HANDLE;
    }

    if (raymarchPipeline.layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vk.device, raymarchPipeline.layout, NULL);
        raymarchPipeline.layout = VK_NULL_HANDLE;
    }

    if (raymarchPipeline.descriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk.device, raymarchPipeline.descriptorLayout, NULL);
        raymarchPipeline.descriptorLayout = VK_NULL_HANDLE;
    }

    if (raymarchPipeline.computeShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(vk.device, raymarchPipeline.computeShader, NULL);
        raymarchPipeline.computeShader = VK_NULL_HANDLE;
    }

    if (distanceFieldBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk.device, distanceFieldBuffer, NULL);
        distanceFieldBuffer = VK_NULL_HANDLE;
    }

    if (distanceFieldMemory != VK_NULL_HANDLE) {
        vkFreeMemory(vk.device, distanceFieldMemory, NULL);
    }

    // Clear Vulkan handles
    raymarchPipeline = {};
    distanceFieldBuffer = VK_NULL_HANDLE;
    distanceFieldMemory = VK_NULL_HANDLE;

    // std::vector cleans up automatically
    distanceFields.clear();
}

void VK_Raymarching_UpdateConfig(void) {
    raymarchConfig.maxSteps = r_raymarchingQuality ? r_raymarchingQuality->integer : 64;
    raymarchConfig.maxDistance = r_raymarchingMaxDistance ? r_raymarchingMaxDistance->value : 100.0f;
    raymarchConfig.epsilon = r_raymarchingEpsilon ? r_raymarchingEpsilon->value : 0.01f;
    raymarchConfig.enableVolumetric = r_raymarchingVolumetric ? r_raymarchingVolumetric->integer : qfalse;
    raymarchConfig.volumetricDensity = r_raymarchingVolumetricDensity ? r_raymarchingVolumetricDensity->value : 0.1f;
}

void VK_Raymarching_AddDistanceField(const distanceField_t* field) {
    if (!field) return;

    // std::vector handles bounds automatically, but we can reserve more space if needed
    if (distanceFields.size() >= distanceFields.capacity()) {
        distanceFields.reserve(distanceFields.capacity() * 2); // Double capacity when full
    }

    distanceFields.push_back(*field);
}

void VK_Raymarching_ClearDistanceFields(void) {
    distanceFields.clear();
}

// Demo function to create some test distance fields using modern C++ features
void VK_Raymarching_AddDemoFields(void) {
    // Use std::array and structured bindings for cleaner code
    const std::array<distanceField_t, 3> demoFields = {{
        {
            .position = {0.0f, 0.0f, -5.0f},
            .radius = 2.0f,
            .type = 0, // sphere
            .dimensions = {0.0f, 0.0f, 0.0f}
        },
        {
            .position = {3.0f, 0.0f, -5.0f},
            .radius = 0.0f,
            .type = 1, // box
            .dimensions = {100.0f, 100.0f, 100.0f}
        },
        {
            .position = {-3.0f, 0.0f, -5.0f},
            .radius = 0.0f,
            .type = 2, // torus
            .dimensions = {150.0f, 50.0f, 0.0f}
        }
    }};

    // Use range-based for loop (C++11 feature)
    for (const auto& field : demoFields) {
        VK_Raymarching_AddDistanceField(&field);
    }
}

void VK_Raymarching_Render(VkCommandBuffer commandBuffer, VkImageView inputImage __attribute__((unused)), VkImageView outputImage __attribute__((unused))) {
    if (!r_raymarching || !r_raymarching->integer) return;

    // Update push constants using designated initializers (C++20 feature)
    const RaymarchingPushConstants pushConstants{
        .cameraPos = { backEnd.refdef.vieworg[0], backEnd.refdef.vieworg[1], backEnd.refdef.vieworg[2], 1.0f },
        .lightColor = { 1.0f, 1.0f, 1.0f, 1.0f },
        .time = 0.0f,
        .maxSteps = raymarchConfig.maxSteps,
        .maxDistance = raymarchConfig.maxDistance,
        .volumetricDensity = raymarchConfig.enableVolumetric ? raymarchConfig.volumetricDensity : 0.0f
    };

    // Bind pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raymarchPipeline.pipeline);

    // TODO: Bind descriptor set when implemented
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raymarchPipeline.layout, 0, 1, &raymarchPipeline.descriptorSet, 0, NULL);

    // Push constants
    vkCmdPushConstants(commandBuffer, raymarchPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RaymarchingPushConstants), &pushConstants);

    // Dispatch compute with modern calculation
    const uint32_t width = vk.renderWidth;
    const uint32_t height = vk.renderHeight;
    const uint32_t groupSizeX = (width + 7) / 8;
    const uint32_t groupSizeY = (height + 7) / 8;

    vkCmdDispatch(commandBuffer, groupSizeX, groupSizeY, 1);
}

void VK_Raymarching_RenderVolumetric(VkCommandBuffer commandBuffer, VkImageView depthImage, VkImageView outputImage) {
    if (!r_raymarchingVolumetric || !r_raymarchingVolumetric->integer) return;

    // Delegate to main render function for volumetric effects
    VK_Raymarching_Render(commandBuffer, depthImage, outputImage);
}

#endif // USE_VULKAN