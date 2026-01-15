/*
=============================================================================
id Tech 3 - Multi-Stage Path Tracer

Ports a multi-stage path tracing architecture to id Tech 3.
Adapts Q2-specific code to Q3's renderer architecture.

Based on multi-stage path tracer reference sources.
=============================================================================
*/

#ifdef USE_VULKAN_RAY_TRACING

#include "../tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "../vk.h"
#include "../vk_rtx_acceleration.h"
#include "../../common/q_shared.h"
#include "../../common/qcommon.h"
#include <algorithm>

// Path tracer pipeline stages (multi-stage pipeline)
typedef enum {
    PT_STAGE_PRIMARY_RAYS,        // Stage 1: Primary rays (G-buffer generation)
    PT_STAGE_REFLECT_REFRACT,     // Stage 2: Reflection/refraction rays
    PT_STAGE_DIRECT_LIGHTING,      // Stage 3: Direct lighting
    PT_STAGE_INDIRECT_LIGHTING_1,  // Stage 4: First indirect bounce
    PT_STAGE_INDIRECT_LIGHTING_2,  // Stage 5: Second indirect bounce
    PT_STAGE_COUNT
} pt_stage_t;

// Path tracer state
typedef struct {
    qboolean initialized;
    qboolean enabled;
    
    // Stage control
    int reflect_refract_iterations;  // Number of reflect/refract passes
    int indirect_bounces;             // Number of indirect bounces (0-2)
    qboolean enable_caustics;         // Enable caustics for direct lighting
    
    // Quality settings
    int samples_per_pixel;
    float temporal_alpha;             // Temporal accumulation weight
    
    // Statistics
    uint64_t total_rays_traced;
    uint64_t total_bounces;
} pt_q2rtx_state_t;

static pt_q2rtx_state_t pt_state = {};

// CVARs
static cvar_t *r_pt_enable = NULL;
static cvar_t *r_pt_reflect_refract = NULL;
static cvar_t *r_pt_indirect_bounces = NULL;
static cvar_t *r_pt_caustics = NULL;
static cvar_t *r_pt_samples = NULL;
static cvar_t *r_pt_temporal_alpha = NULL;

// Pipeline handles (will be created)
static VkPipeline pt_pipelines[PT_STAGE_COUNT] = {VK_NULL_HANDLE};
static VkPipelineLayout pt_pipeline_layout = VK_NULL_HANDLE;
static VkDescriptorSetLayout pt_descriptor_layout = VK_NULL_HANDLE;
static VkDescriptorSet pt_descriptor_sets[MAX_SWAPCHAIN_IMAGES] = {VK_NULL_HANDLE};

// Buffers for multi-stage rendering
typedef struct {
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
    uint32_t width;
    uint32_t height;
} pt_image_t;

static pt_image_t gbuffer_image = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0 };           // G-buffer (visibility buffer)
static pt_image_t direct_lighting_image = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0 };   // Direct lighting result
static pt_image_t indirect_lighting_image = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0 }; // Indirect lighting result
static pt_image_t final_image = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0 };             // Final composited result

/*
===============
PT_Q2RTX_Init

Initialize multi-stage path tracer
===============
*/
void PT_Q2RTX_Init(void)
{
    if (pt_state.initialized) {
        return;
    }
    
    // Register CVARs
    r_pt_enable = ri.Cvar_Get("r_pt_enable", "1", CVAR_ARCHIVE | CVAR_LATCH);
    r_pt_reflect_refract = ri.Cvar_Get("r_pt_reflect_refract", "2", CVAR_ARCHIVE);
    r_pt_indirect_bounces = ri.Cvar_Get("r_pt_indirect_bounces", "2", CVAR_ARCHIVE);
    r_pt_caustics = ri.Cvar_Get("r_pt_caustics", "1", CVAR_ARCHIVE);
    r_pt_samples = ri.Cvar_Get("r_pt_samples", "1", CVAR_ARCHIVE);
    r_pt_temporal_alpha = ri.Cvar_Get("r_pt_temporal_alpha", "0.9", CVAR_ARCHIVE);
    
    // Initialize state
    memset(&pt_state, 0, sizeof(pt_q2rtx_state_t));
    pt_state.enabled = (r_pt_enable->integer != 0);
    pt_state.reflect_refract_iterations = Com_Clamp(0, 4, r_pt_reflect_refract->integer);
    pt_state.indirect_bounces = Com_Clamp(0, 2, r_pt_indirect_bounces->integer);
    pt_state.enable_caustics = (r_pt_caustics->integer != 0);
    pt_state.samples_per_pixel = Com_Clamp(1, 8, r_pt_samples->integer);
    pt_state.temporal_alpha = Com_Clamp(0.0f, 1.0f, r_pt_temporal_alpha->value);
    
    pt_state.initialized = qtrue;
    
    Com_Printf("Q2RTX-style path tracer initialized\n");
    Com_Printf("  Reflect/Refract iterations: %d\n", pt_state.reflect_refract_iterations);
    Com_Printf("  Indirect bounces: %d\n", pt_state.indirect_bounces);
    Com_Printf("  Caustics: %s\n", pt_state.enable_caustics ? "enabled" : "disabled");
    Com_Printf("  Samples per pixel: %d\n", pt_state.samples_per_pixel);
}

/*
===============
PT_Q2RTX_Shutdown

Shutdown multi-stage path tracer
===============
*/
void PT_Q2RTX_Shutdown(void)
{
    if (!pt_state.initialized) {
        return;
    }
    
    // TODO: Destroy pipelines, descriptor sets, images, etc.
    
    pt_state.initialized = qfalse;
    Com_Printf("Q2RTX-style path tracer shutdown\n");
}

/*
===============
PT_Q2RTX_CreateImages

Create images for multi-stage path tracing
===============
*/
static void PT_Q2RTX_CreateImages(uint32_t width, uint32_t height)
{
    // TODO: Create G-buffer, direct lighting, indirect lighting, and final images
    // This will be implemented when integrating with the renderer
    
    gbuffer_image.width = width;
    gbuffer_image.height = height;
    direct_lighting_image.width = width;
    direct_lighting_image.height = height;
    indirect_lighting_image.width = width;
    indirect_lighting_image.height = height;
    final_image.width = width;
    final_image.height = height;
}

/*
===============
PT_Q2RTX_Stage_PrimaryRays

Stage 1: Primary rays - generate G-buffer
===============
*/
static void PT_Q2RTX_Stage_PrimaryRays(uint32_t width, uint32_t height)
{
    // This stage shoots primary rays from the camera and generates:
    // - Visibility buffer (surface information)
    // - Motion vectors (for temporal accumulation)
    // - Texture gradients (for denoising)
    // - Transparency information (sprites, particles)
    
    if (!vk.rayTracingSupported || !vk.rt.initialized || vk.rt.raytracingPipeline == VK_NULL_HANDLE) {
        return;
    }
    
    // Use existing ray tracing pipeline for primary rays
    // Bind pipeline and descriptor set
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vk.rt.raytracingPipeline);
    
    qvkCmdBindDescriptorSets(
        vk.cmd->command_buffer,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        vk.rt.raytracingPipelineLayout,
        0,
        1,
        &vk.rt.raytracingDescriptorSet,
        0,
        NULL
    );
    
    // Set push constants for primary rays stage
    uint32_t push_constants[7] = {
        1,  // max_recursion_depth (primary ray only)
        (uint32_t)pt_state.samples_per_pixel,
        0,  // enable_shadows (handled in direct lighting)
        0,  // enable_reflections (handled in reflect/refract stage)
        0,  // enable_gi (handled in indirect stage)
        1,  // gi_samples
        1   // enable_path_tracing (primary rays)
    };
    
    qvkCmdPushConstants(
        vk.cmd->command_buffer,
        vk.rt.raytracingPipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
        0,
        sizeof(push_constants),
        push_constants
    );
    
    // Dispatch primary rays
    qvkCmdTraceRaysKHR(
        vk.cmd->command_buffer,
        &vk.rt.raygenShaderBindingTable,
        &vk.rt.missShaderBindingTable,
        &vk.rt.hitShaderBindingTable,
        &vk.rt.callableShaderBindingTable,
        width,
        height,
        1
    );
    
    // Memory barrier to ensure primary rays complete before next stage
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
    };
    
    qvkCmdPipelineBarrier(
        vk.cmd->command_buffer,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        1, &barrier,
        0, NULL,
        0, NULL
    );
    
    pt_state.total_rays_traced += width * height;
}

/*
===============
PT_Q2RTX_Stage_ReflectRefract

Stage 2: Reflection/refraction rays
===============
*/
static void PT_Q2RTX_Stage_ReflectRefract(uint32_t width, uint32_t height, int iteration)
{
    // This stage handles recursive reflections and refractions for:
    // - Water surfaces
    // - Glass/mirror surfaces
    // - Security camera screens
    
    // Uses checkerboard rendering for multi-GPU support
    // Can be executed multiple times for recursive reflections
    
    if (!vk.rayTracingSupported || !vk.rt.initialized || vk.rt.raytracingPipeline == VK_NULL_HANDLE) {
        return;
    }
    
    // Bind pipeline and descriptor set
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vk.rt.raytracingPipeline);
    
    qvkCmdBindDescriptorSets(
        vk.cmd->command_buffer,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        vk.rt.raytracingPipelineLayout,
        0,
        1,
        &vk.rt.raytracingDescriptorSet,
        0,
        NULL
    );
    
    // Set push constants for reflect/refract stage
    // Max recursion increases with iteration for recursive reflections
    uint32_t max_recursion = (uint32_t)(iteration + 2); // +2 for primary + reflect/refract
    uint32_t push_constants[7] = {
        max_recursion,
        (uint32_t)pt_state.samples_per_pixel,
        0,  // enable_shadows
        1,  // enable_reflections
        0,  // enable_gi
        1,  // gi_samples
        1   // enable_path_tracing
    };
    
    qvkCmdPushConstants(
        vk.cmd->command_buffer,
        vk.rt.raytracingPipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
        0,
        sizeof(push_constants),
        push_constants
    );
    
    // Dispatch reflect/refract rays
    qvkCmdTraceRaysKHR(
        vk.cmd->command_buffer,
        &vk.rt.raygenShaderBindingTable,
        &vk.rt.missShaderBindingTable,
        &vk.rt.hitShaderBindingTable,
        &vk.rt.callableShaderBindingTable,
        width,
        height,
        1
    );
    
    // Memory barrier
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
    };
    
    qvkCmdPipelineBarrier(
        vk.cmd->command_buffer,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        1, &barrier,
        0, NULL,
        0, NULL
    );
    
    pt_state.total_rays_traced += width * height;
}

/*
===============
PT_Q2RTX_Stage_DirectLighting

Stage 3: Direct lighting
===============
*/
static void PT_Q2RTX_Stage_DirectLighting(uint32_t width, uint32_t height)
{
    // This stage computes direct lighting for surfaces in the G-buffer:
    // - Local polygonal lights
    // - Sphere lights
    // - Sun light (directional)
    // - Optional caustics for glass/water
    
    // TODO: Dispatch direct lighting ray generation shader
    // This samples light sources and computes direct illumination
    
    pt_state.total_rays_traced += width * height;
}

/*
===============
PT_Q2RTX_Stage_IndirectLighting

Stage 4/5: Indirect lighting (bounce lighting)
===============
*/
static void PT_Q2RTX_Stage_IndirectLighting(uint32_t width, uint32_t height, int bounce)
{
    // This stage computes indirect lighting (global illumination):
    // - Takes surface from G-buffer (or previous bounce)
    // - Traces a single bounce ray (diffuse or specular)
    // - Computes direct lighting at the hit surface
    // - Updates G-buffer with new surface for next bounce
    
    // First bounce: can be diffuse or specular (based on material)
    // Second bounce: only diffuse (for performance)
    // Second bounce: no local lights (too expensive, barely noticeable)
    
    if (!vk.rayTracingSupported || !vk.rt.initialized || vk.rt.raytracingPipeline == VK_NULL_HANDLE) {
        return;
    }
    
    // Bind pipeline and descriptor set
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vk.rt.raytracingPipeline);
    
    qvkCmdBindDescriptorSets(
        vk.cmd->command_buffer,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        vk.rt.raytracingPipelineLayout,
        0,
        1,
        &vk.rt.raytracingDescriptorSet,
        0,
        NULL
    );
    
    // Set push constants for indirect lighting stage
    // Max recursion: primary + reflect/refract + indirect bounces
    uint32_t max_recursion = (uint32_t)(3 + bounce); // primary + reflect/refract + bounces
    uint32_t push_constants[7] = {
        max_recursion,
        (uint32_t)pt_state.samples_per_pixel,
        1,  // enable_shadows (for indirect lighting)
        0,  // enable_reflections
        1,  // enable_gi
        (uint32_t)pt_state.samples_per_pixel,  // gi_samples
        1   // enable_path_tracing
    };
    
    qvkCmdPushConstants(
        vk.cmd->command_buffer,
        vk.rt.raytracingPipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
        0,
        sizeof(push_constants),
        push_constants
    );
    
    // Dispatch indirect lighting rays
    // For second bounce, reduce resolution for performance
    uint32_t dispatch_height = (bounce == 2) ? (height / 2) : height;
    
    qvkCmdTraceRaysKHR(
        vk.cmd->command_buffer,
        &vk.rt.raygenShaderBindingTable,
        &vk.rt.missShaderBindingTable,
        &vk.rt.hitShaderBindingTable,
        &vk.rt.callableShaderBindingTable,
        width,
        dispatch_height,
        1
    );
    
    // Memory barrier
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
    };
    
    qvkCmdPipelineBarrier(
        vk.cmd->command_buffer,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        1, &barrier,
        0, NULL,
        0, NULL
    );
    
    pt_state.total_rays_traced += width * dispatch_height;
    pt_state.total_bounces += width * dispatch_height;
}

/*
===============
PT_Q2RTX_Render

Main rendering function - executes all stages
===============
*/
void PT_Q2RTX_Render(uint32_t width, uint32_t height)
{
    if (!pt_state.initialized || !pt_state.enabled) {
        return;
    }
    
    if (!vk.rayTracingSupported || !vk.rt.initialized) {
        return;
    }
    
    // Create images if needed
    if (gbuffer_image.width != width || gbuffer_image.height != height) {
        PT_Q2RTX_CreateImages(width, height);
    }
    
    // Stage 1: Primary rays (G-buffer generation)
    PT_Q2RTX_Stage_PrimaryRays(width, height);
    
    // Stage 2: Reflection/refraction (can be multiple iterations)
    for (int i = 0; i < pt_state.reflect_refract_iterations; i++) {
        PT_Q2RTX_Stage_ReflectRefract(width, height, i);
        
        // TODO: Execute god rays shader between reflect/refract iterations
        // This accumulates volumetric lighting along the ray
    }
    
    // Stage 3: Direct lighting
    PT_Q2RTX_Stage_DirectLighting(width, height);
    
    // Stage 4: First indirect bounce
    if (pt_state.indirect_bounces >= 1) {
        PT_Q2RTX_Stage_IndirectLighting(width, height, 1);
    }
    
    // Stage 5: Second indirect bounce
    if (pt_state.indirect_bounces >= 2) {
        PT_Q2RTX_Stage_IndirectLighting(width, height, 2);
    }
    
    // TODO: Composite all stages into final image
    // This combines direct + indirect lighting with proper weighting
}

/*
===============
PT_Q2RTX_UpdateCVARs

Update path tracer state from CVARs
===============
*/
void PT_Q2RTX_UpdateCVARs(void)
{
    if (!pt_state.initialized) {
        return;
    }
    
    pt_state.enabled = (r_pt_enable->integer != 0);
    pt_state.reflect_refract_iterations = Com_Clamp(0, 4, r_pt_reflect_refract->integer);
    pt_state.indirect_bounces = Com_Clamp(0, 2, r_pt_indirect_bounces->integer);
    pt_state.enable_caustics = (r_pt_caustics->integer != 0);
    pt_state.samples_per_pixel = Com_Clamp(1, 8, r_pt_samples->integer);
    pt_state.temporal_alpha = Com_Clamp(0.0f, 1.0f, r_pt_temporal_alpha->value);
}

/*
===============
PT_Q2RTX_GetStats

Get path tracer statistics
===============
*/
void PT_Q2RTX_GetStats(uint64_t *total_rays, uint64_t *total_bounces)
{
    if (total_rays) {
        *total_rays = pt_state.total_rays_traced;
    }
    if (total_bounces) {
        *total_bounces = pt_state.total_bounces;
    }
}

#endif // USE_VULKAN_RAY_TRACING
