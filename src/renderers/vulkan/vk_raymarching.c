/*
=============================================================================
Vulkan Raymarching Implementation

Core raymarching functionality for advanced rendering effects.
=============================================================================
*/

#ifdef USE_VULKAN

#include "vk_raymarching.h"
#include "vk.h"
#include "tr_local.h"
#include "vk_shaders.h"
#include <stdlib.h>
#include <string.h>

// Global raymarching state
static raymarchingConfig_t raymarchConfig;
static raymarchingPipeline_t raymarchPipeline;
static VkBuffer distanceFieldBuffer = VK_NULL_HANDLE;
static VkDeviceMemory distanceFieldMemory = VK_NULL_HANDLE;
static distanceField_t* distanceFields = NULL;
static int numDistanceFields = 0;
static int maxDistanceFields = 0;

// Push constants for raymarching shader
typedef struct {
    vec4_t cameraPos;
    mat4_t viewProjInverse;
    vec4_t lightDirection;
    vec4_t lightColor;
    float time;
    int maxSteps;
    float maxDistance;
    float epsilon;
    float volumetricDensity;
    int numFields;
} RaymarchingPushConstants;

static const char* raymarchingComputeShader = R"glsl(
#version 450
#extension GL_GOOGLE_include_directive : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D inputImage;
layout(set = 0, binding = 1, rgba8) uniform image2D outputImage;
layout(set = 0, binding = 2) buffer DistanceFields {
    vec4 positions[256];  // xyz = position, w = radius
    ivec4 types[256];     // x = type, yzw = dimensions
} distanceFields;

layout(push_constant) uniform PushConstants {
    vec4 cameraPos;
    mat4 viewProjInverse;
    vec4 lightDirection;
    vec4 lightColor;
    float time;
    int maxSteps;
    float maxDistance;
    float epsilon;
    float volumetricDensity;
    int numFields;
} pc;

// Distance functions
float sdSphere(vec3 p, float radius) {
    return length(p) - radius;
}

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

// Scene distance function
float map(vec3 p) {
    float d = pc.maxDistance;

    for (int i = 0; i < pc.numFields; i++) {
        vec3 pos = distanceFields.positions[i].xyz;
        float radius = distanceFields.positions[i].w;
        vec3 localP = p - pos;

        float dist;
        int type = distanceFields.types[i].x;

        if (type == 0) { // Sphere
            dist = sdSphere(localP, radius);
        } else if (type == 1) { // Box
            vec3 dims = vec3(distanceFields.types[i].yzw) * 0.01;
            dist = sdBox(localP, dims);
        } else if (type == 2) { // Torus
            vec2 dims = vec2(distanceFields.types[i].y, distanceFields.types[i].z) * 0.01;
            dist = sdTorus(localP, dims);
        } else {
            dist = pc.maxDistance;
        }

        d = min(d, dist);
    }

    return d;
}

// Raymarching function
float raymarch(vec3 ro, vec3 rd) {
    float t = 0.0;
    for (int i = 0; i < pc.maxSteps; i++) {
        vec3 p = ro + rd * t;
        float d = map(p);
        if (d < pc.epsilon) {
            return t;
        }
        t += d;
        if (t > pc.maxDistance) {
            break;
        }
    }
    return -1.0;
}

// Normal calculation
vec3 calcNormal(vec3 p) {
    vec2 e = vec2(1.0, -1.0) * 0.5773 * 0.0005;
    return normalize(
        e.xyy * map(p + e.xyy) +
        e.yyx * map(p + e.yyx) +
        e.yxy * map(p + e.yxy) +
        e.xxx * map(p + e.xxx)
    );
}

// Volumetric raymarching for fog/clouds
vec3 raymarchVolumetric(vec3 ro, vec3 rd, float maxDist) {
    vec3 color = vec3(0.0);
    float t = 0.0;
    float density = pc.volumetricDensity;

    for (int i = 0; i < 64; i++) { // Fixed steps for volumetric
        vec3 p = ro + rd * t;
        float d = map(p);

        if (d < 0.0) { // Inside volume
            float transmittance = exp(-density * t);
            color += vec3(0.8, 0.9, 1.0) * density * transmittance * 0.1;
        }

        t += maxDist / 64.0;
        if (t > maxDist) break;
    }

    return color;
}

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = vec2(pixelCoord) / vec2(imageSize(outputImage));

    // Reconstruct world position from depth
    vec3 ro = pc.cameraPos.xyz;
    vec3 rd = normalize(mat3(pc.viewProjInverse) * vec3(uv * 2.0 - 1.0, 1.0));

    vec4 inputColor = texture(inputImage, uv);

    if (pc.volumetricDensity > 0.0) {
        // Volumetric raymarching
        vec3 volumetricColor = raymarchVolumetric(ro, rd, pc.maxDistance);
        vec4 finalColor = inputColor + vec4(volumetricColor, 0.0);
        imageStore(outputImage, pixelCoord, finalColor);
    } else {
        // Surface raymarching
        float t = raymarch(ro, rd);

        if (t > 0.0) {
            vec3 p = ro + rd * t;
            vec3 n = calcNormal(p);

            // Simple shading
            float diff = max(dot(n, normalize(-pc.lightDirection.xyz)), 0.0);
            vec3 color = pc.lightColor.rgb * diff + vec3(0.1); // Ambient

            vec4 finalColor = vec4(color, 1.0);
            imageStore(outputImage, pixelCoord, finalColor);
        } else {
            // Background
            imageStore(outputImage, pixelCoord, inputColor);
        }
    }
}
)glsl";

qboolean VK_Raymarching_Init(void) {
    Com_Printf("Initializing Vulkan Raymarching...\n");

    // Initialize configuration
    raymarchConfig.maxSteps = r_raymarchingQuality->integer;
    raymarchConfig.maxDistance = r_raymarchingMaxDistance->value;
    raymarchConfig.epsilon = r_raymarchingEpsilon->value;
    raymarchConfig.enableVolumetric = r_raymarchingVolumetric->integer;
    raymarchConfig.volumetricDensity = r_raymarchingVolumetricDensity->value;

    // Initialize distance fields buffer
    maxDistanceFields = 256;
    distanceFields = malloc(sizeof(distanceField_t) * maxDistanceFields);
    numDistanceFields = 0;

    // Create shader module
    VkShaderModuleCreateInfo shaderInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = strlen(raymarchingComputeShader),
        .pCode = (const uint32_t*)raymarchingComputeShader
    };

    VK_CHECK(vkCreateShaderModule(vk.device, &shaderInfo, NULL, &raymarchPipeline.computeShader));

    // Create descriptor set layout
    VkDescriptorSetLayoutBinding bindings[] = {
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
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARRAY_LEN(bindings),
        .pBindings = bindings
    };

    VK_CHECK(vkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &raymarchPipeline.descriptorLayout));

    // Create pipeline layout
    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(RaymarchingPushConstants)
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &raymarchPipeline.descriptorLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VK_CHECK(vkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &raymarchPipeline.layout));

    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = raymarchPipeline.computeShader,
            .pName = "main"
        },
        .layout = raymarchPipeline.layout
    };

    VK_CHECK(vkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &raymarchPipeline.pipeline));

    // Add demo distance fields for testing
    VK_Raymarching_AddDemoFields();

    Com_Printf("Vulkan Raymarching initialized successfully\n");
    return qtrue;
}

void VK_Raymarching_Shutdown(void) {
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
        distanceFieldMemory = VK_NULL_HANDLE;
    }

    if (distanceFields) {
        free(distanceFields);
        distanceFields = NULL;
    }

    numDistanceFields = 0;
    maxDistanceFields = 0;
}

void VK_Raymarching_UpdateConfig(void) {
    raymarchConfig.maxSteps = r_raymarchingQuality->integer;
    raymarchConfig.maxDistance = r_raymarchingMaxDistance->value;
    raymarchConfig.epsilon = r_raymarchingEpsilon->value;
    raymarchConfig.enableVolumetric = r_raymarchingVolumetric->integer;
    raymarchConfig.volumetricDensity = r_raymarchingVolumetricDensity->value;
}

void VK_Raymarching_AddDistanceField(const distanceField_t* field) {
    if (numDistanceFields >= maxDistanceFields) {
        Com_Printf(S_COLOR_YELLOW "Warning: Maximum distance fields reached\n");
        return;
    }

    distanceFields[numDistanceFields++] = *field;
}

void VK_Raymarching_ClearDistanceFields(void) {
    numDistanceFields = 0;
}

// Demo function to create some test distance fields
void VK_Raymarching_AddDemoFields(void) {
    distanceField_t sphere = {
        .position = {0.0f, 0.0f, -5.0f},
        .radius = 2.0f,
        .type = 0, // sphere
        .dimensions = {0.0f, 0.0f, 0.0f}
    };
    VK_Raymarching_AddDistanceField(&sphere);

    distanceField_t box = {
        .position = {3.0f, 0.0f, -5.0f},
        .radius = 0.0f,
        .type = 1, // box
        .dimensions = {100, 100, 100} // scaled dimensions
    };
    VK_Raymarching_AddDistanceField(&box);

    distanceField_t torus = {
        .position = {-3.0f, 0.0f, -5.0f},
        .radius = 0.0f,
        .type = 2, // torus
        .dimensions = {150, 50, 0} // major/minor radius
    };
    VK_Raymarching_AddDistanceField(&torus);
}

void VK_Raymarching_Render(VkCommandBuffer commandBuffer, VkImageView inputImage, VkImageView outputImage) {
    if (!r_raymarching->integer) return;

    // Update push constants
    RaymarchingPushConstants pushConstants = {
        .cameraPos = { vk.viewOrigin[0], vk.viewOrigin[1], vk.viewOrigin[2], 1.0f },
        .viewProjInverse = { 0 }, // Would need to compute inverse view-projection matrix
        .lightDirection = { 0.0f, 0.0f, -1.0f, 0.0f },
        .lightColor = { 1.0f, 1.0f, 1.0f, 1.0f },
        .time = 0.0f,
        .maxSteps = raymarchConfig.maxSteps,
        .maxDistance = raymarchConfig.maxDistance,
        .epsilon = raymarchConfig.epsilon,
        .volumetricDensity = raymarchConfig.enableVolumetric ? raymarchConfig.volumetricDensity : 0.0f,
        .numFields = numDistanceFields
    };

    // Bind pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raymarchPipeline.pipeline);

    // Bind descriptor set (would need to be allocated and updated)
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raymarchPipeline.layout, 0, 1, &raymarchPipeline.descriptorSet, 0, NULL);

    // Push constants
    vkCmdPushConstants(commandBuffer, raymarchPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RaymarchingPushConstants), &pushConstants);

    // Dispatch compute
    uint32_t width = vk.swapchainExtent.width;
    uint32_t height = vk.swapchainExtent.height;
    vkCmdDispatch(commandBuffer, (width + 7) / 8, (height + 7) / 8, 1);
}

void VK_Raymarching_RenderVolumetric(VkCommandBuffer commandBuffer, VkImageView depthImage, VkImageView outputImage) {
    if (!r_raymarchingVolumetric->integer) return;

    // Similar to main render function but focused on volumetric effects
    VK_Raymarching_Render(commandBuffer, depthImage, outputImage);
}

#endif // USE_VULKAN