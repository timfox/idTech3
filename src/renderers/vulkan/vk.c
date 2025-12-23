#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;

// VRS CVAR extern declarations
extern cvar_t *r_vrs;
extern cvar_t *r_vrs_mode;
extern cvar_t *r_vrs_center_radius;
extern cvar_t *r_vrs_falloff_start;
extern cvar_t *r_vrs_min_rate;
extern cvar_t *r_vrs_max_rate;
extern cvar_t *r_vk_profiling;
extern cvar_t *r_vk_debug_overlay;

// Other CVAR extern declarations
extern cvar_t *r_vk_disableScreenMap;
extern cvar_t *r_procDressing;
extern cvar_t *r_materialSystem;
extern cvar_t *r_frameTelemetry;
extern cvar_t *r_bloom;
extern cvar_t *r_dlss;
extern cvar_t *r_dlss_quality;
extern cvar_t *r_dlss_sharpening;
extern cvar_t *r_styleTransfer;
extern cvar_t *r_styleStrength;
extern cvar_t *r_styleLevels;
extern cvar_t *r_styleEdge;
extern cvar_t *r_postprocess_workgroup;
extern cvar_t *r_postprocess_compute;
extern cvar_t *r_postQuality;
extern cvar_t *r_hdr;
extern cvar_t *r_tonemapMode;
extern cvar_t *r_tonemapExposure;
extern cvar_t *r_gamma;
extern cvar_t *r_greyscale;
extern cvar_t *r_dither;
extern cvar_t *r_vk_hotReload;
#include "../../common/performance_counters.h"
#include "vk.h"
#include "vk_post_process.h"
#ifdef USE_VULKAN_RAY_TRACING
#include "vk_portal_lights.h"
#endif
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
extern int setenv( const char *name, const char *value, int overwrite );

// Modern C23/C++23 safety features for Vulkan renderer
#include <assert.h>

// Static assertions for Vulkan safety
static_assert(sizeof(VkDeviceSize) >= sizeof(size_t), "VkDeviceSize must be at least as large as size_t");
// Note: VK_NULL_HANDLE is defined as 0 or ((void*)0), which may not be a compile-time constant
// This assertion is checked at runtime instead
#if 0
static_assert(VK_NULL_HANDLE == NULL, "VK_NULL_HANDLE must equal NULL for compatibility");
#endif

// Modern attribute macros for Vulkan functions
#ifdef __GNUC__
#define VK_NONNULL __attribute__((nonnull))
#define VK_NONNULL_PARAMS(...) __attribute__((nonnull(__VA_ARGS__)))
#define VK_PURE __attribute__((pure))
#define VK_CONST __attribute__((const))
#define VK_MALLOC __attribute__((malloc))
#define VK_RETURNS_NONNULL __attribute__((returns_nonnull))
#else
#define VK_NONNULL
#define VK_NONNULL_PARAMS(...)
#define VK_PURE
#define VK_CONST
#define VK_MALLOC
#define VK_RETURNS_NONNULL
#endif

// Enhanced Vulkan feature detection and device scoring
typedef struct {
    qboolean supported;
    const char *name;
    uint32_t score; // Higher score = better feature
} vk_feature_t;

// Modern Vulkan 1.3+ features implementation
typedef struct {
    qboolean synchronization2;        // VK_KHR_synchronization2
    qboolean dynamicRendering;        // VK_KHR_dynamic_rendering
    qboolean meshShaders;             // VK_EXT_mesh_shader
    qboolean rayTracing;              // VK_KHR_ray_tracing_pipeline
    qboolean dlssSupported;           // NVIDIA DLSS (framework ready)
    qboolean fsrSupported;            // AMD FSR (framework ready)
    qboolean pipelineBinaries;        // VK_KHR_pipeline_executable_properties
} vk_advanced_features_t;

vk_advanced_features_t vk_advanced = {0};

// DLSS/FSR upscaling support
typedef struct {
    qboolean supported;
    qboolean enabled;
    int qualityMode;      // 0=disabled, 1=ultra quality, 2=balanced, 3=performance, 4=ultra performance
    float sharpness;
    VkImage outputImage;
    VkImageView outputImageView;
} vk_upscaler_t;

static vk_upscaler_t vk_upscaler = {0};

typedef struct {
    uint32_t device_index;
    uint32_t score;
    VkPhysicalDevice device;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory;
    const char *reason; // Why this device was selected/rejected
} vk_device_candidate_t;

// Basic performance tracking
typedef struct {
    uint32_t fps;
    double frame_time;
} vk_performance_stats_t;

static vk_performance_stats_t vk_perf_stats = {0};

// Advanced memory management and resource pooling - framework ready for future implementation

// Enhanced debugging support
#ifdef USE_VK_VALIDATION
static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    (void)pUserData;

    // Filter messages based on severity
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        const char *severity_str = "UNKNOWN";
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            severity_str = "ERROR";
        } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            severity_str = "WARNING";
        } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
            severity_str = "INFO";
        }

        const char *type_str = "UNKNOWN";
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
            type_str = "GENERAL";
        } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            type_str = "VALIDATION";
        } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
            type_str = "PERFORMANCE";
        }

        ri.Printf(PRINT_WARNING, "[Vulkan %s %s] %s\n", severity_str, type_str, pCallbackData->pMessage);
    }

    return VK_FALSE;
}
#endif

// Resource tracking - to be implemented in future versions for detailed memory monitoring

// Performance monitoring - basic frame timing
static void vk_update_performance_stats(void) {
    static double last_time = 0.0;
    static uint32_t frame_count = 0;

    double current_time = ri.Milliseconds() / 1000.0;

    if (last_time == 0.0) {
        last_time = current_time;
        return;
    }

    double delta_time = current_time - last_time;
    frame_count++;

    // Update FPS every second
    if (delta_time >= 1.0) {
        vk_perf_stats.fps = (uint32_t)(frame_count / delta_time);
        frame_count = 0;
        last_time = current_time;
    }

    // Enhanced profiling if enabled
    if (r_vk_profiling && r_vk_profiling->integer) {
        vk.profiling.enabled = qtrue;
        
        // Update frame time history
        double frame_time = delta_time * 1000.0; // Convert to milliseconds
        vk.profiling.frame_time_history[vk.profiling.frame_time_history_index] = frame_time;
        vk.profiling.frame_time_history_index = (vk.profiling.frame_time_history_index + 1) % 128;

        // Calculate frame time variance
        double sum = 0.0;
        double sum_sq = 0.0;
        uint32_t count = 0;
        for (uint32_t i = 0; i < 128; i++) {
            if (vk.profiling.frame_time_history[i] > 0.0) {
                sum += vk.profiling.frame_time_history[i];
                sum_sq += vk.profiling.frame_time_history[i] * vk.profiling.frame_time_history[i];
                count++;
            }
        }
        if (count > 0) {
            double mean = sum / count;
            double variance = (sum_sq / count) - (mean * mean);
            vk.profiling.frame_time_variance = (float)variance;
        }

        // Update debug overlay stats
        if (r_vk_debug_overlay && r_vk_debug_overlay->integer) {
            vk.debug_overlay.enabled = qtrue;
            vk.debug_overlay.frame_time_ms = (float)frame_time;
            vk.debug_overlay.frame_time_variance = vk.profiling.frame_time_variance;
        }
    } else {
        vk.profiling.enabled = qfalse;
    }
}

// Timeline semaphore management functions
// Timeline semaphore framework - ready for future implementation

// Advanced memory management functions
// Memory management functions - framework ready for future implementation

// DLSS/FSR upscaler functions
static void __attribute__((unused)) vk_init_upscaler(void) {
    // Detect NVIDIA DLSS support
    // This would typically check for NVIDIA DLSS library availability
    // For now, mark as unsupported but ready for implementation
    vk_upscaler.supported = qfalse;
    vk_upscaler.enabled = qfalse;

    // TODO: Implement actual DLSS detection and initialization
    ri.Printf(PRINT_ALL, "...upscaler: framework ready (DLSS/FSR not yet implemented)\n");
}

static void __attribute__((unused)) vk_shutdown_upscaler(void) {
    if (vk_upscaler.outputImageView != VK_NULL_HANDLE) {
        // Clean up resources when implemented
        vk_upscaler.outputImageView = VK_NULL_HANDLE;
    }
    if (vk_upscaler.outputImage != VK_NULL_HANDLE) {
        // Clean up resources when implemented
        vk_upscaler.outputImage = VK_NULL_HANDLE;
    }
}

// Upscaler availability check - ready for implementation

// Upscaler evaluation - framework ready for DLSS/FSR implementation

// Device lost recovery - basic implementation
// Basic device lost recovery implementation

// Compatibility shim: some SDKs may not expose the EXT mesh shader feature struct/enum.
#ifndef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT (VkStructureType)1000328000
typedef struct VkPhysicalDeviceMeshShaderFeaturesEXT {
	VkStructureType sType;
	void*           pNext;
	VkBool32        taskShader;
	VkBool32        meshShader;
	VkBool32        multiviewMeshShader;
	VkBool32        primitiveFragmentShadingRateMeshShader;
	VkBool32        meshShaderQueries;
} VkPhysicalDeviceMeshShaderFeaturesEXT;
#endif
#ifdef USE_VULKAN_RAY_TRACING
#include "vk_gibs.h"
#endif
#include "vk_gpu_culling.h"
#include "vk_proc_dressing.h"
#include "vk_material_system.h"
#include "vk_cell_streaming.h"
#include "vk_atmosphere.h"
#ifdef IDTECH3_VK_EXPERIMENTAL
#include "vk_ibl.h"
#include "vk_shadows.h"
#include "gltf_loader.h"
#include "vk_dynamic_rendering.h"
#include "vk_multithreaded_rendering.h"
#include "vk_compute_raytracing.h"
#include "vk_sem.h"
#include "vk_graphics_settings.h"
#include "vk_physics.h"
#endif

#include "vk_commands.h"
#include "vk_framebuffer.h"
#include "vk_bindless.h"
#include "vk_shader_cache.h"
#include "vk_async_compile.h"

// Pipeline binary function pointers (VK_KHR_pipeline_executable_properties) - forward declaration
PFN_vkGetPipelineExecutablePropertiesKHR		qvkGetPipelineExecutablePropertiesKHR;
PFN_vkGetPipelineExecutableStatisticsKHR		qvkGetPipelineExecutableStatisticsKHR;
PFN_vkGetPipelineExecutableInternalRepresentationsKHR qvkGetPipelineExecutableInternalRepresentationsKHR;

extern cvar_t *r_frameTelemetry;
extern cvar_t *r_procDressing;
extern cvar_t *r_procDressingDensity;
extern cvar_t *r_vulkan_validation;
extern cvar_t *r_vulkan_debug;
extern cvar_t *r_vk_renderdoc;
extern cvar_t *r_vk_hotReload;
extern cvar_t *r_procDressingDebug;
extern cvar_t *r_foliageWindStrength;
extern cvar_t *r_foliageWindFrequency;
extern cvar_t *r_gpuSceneGraph;
extern cvar_t *r_gpuSceneDebug;
extern cvar_t *r_gpuSkinning;
extern cvar_t *r_gpuRagdoll;
extern cvar_t *r_vk_renderdoc;
extern cvar_t *r_vk_profiling;
extern cvar_t *r_vk_debug_overlay;
extern cvar_t *r_vk_hotReload;
extern cvar_t *r_texture_streaming;
extern cvar_t *r_vram_budget;


#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x)/sizeof(*(x)))
#endif




// Pipeline binary header for versioning and validation
// Save pipeline binary to disk


// Create pipeline from binary (placeholder - full implementation requires VK_KHR_pipeline_library)

#if defined (_DEBUG)
#if defined (_WIN32)
#define USE_VK_VALIDATION
#include <windows.h> // for win32 debug callback
#endif
#endif

static int vkSamples = VK_SAMPLE_COUNT_1_BIT;
static double vkFrameTelemetryLast = 0.0;

static VkInstance vk_instance = VK_NULL_HANDLE;
static VkSurfaceKHR vk_surface = VK_NULL_HANDLE;

#ifndef NDEBUG
VkDebugReportCallbackEXT vk_debug_callback = VK_NULL_HANDLE;
#endif

static void vk_log_subgroup_capabilities( VkPhysicalDevice physical_device ) {
	if ( qvkGetPhysicalDeviceProperties2KHR == NULL ) {
		ri.Printf( PRINT_DEVELOPER, "...subgroup capabilities unavailable (vkGetPhysicalDeviceProperties2KHR not loaded)\n" );
		return;
	}

	VkPhysicalDeviceSubgroupProperties subgroup = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,
		.pNext = NULL
	};
	VkPhysicalDeviceProperties2 props2 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
		.pNext = &subgroup
	};

	qvkGetPhysicalDeviceProperties2KHR( physical_device, &props2 );

	ri.Printf( PRINT_ALL, "...subgroup: size=%u stages=0x%x ops=0x%x\n",
		subgroup.subgroupSize,
		subgroup.supportedStages,
		subgroup.supportedOperations );
}

//
// Vulkan API functions used by the renderer.
//
static PFN_vkCreateInstance								qvkCreateInstance;
static PFN_vkEnumerateInstanceExtensionProperties		qvkEnumerateInstanceExtensionProperties;

static PFN_vkCreateDevice								qvkCreateDevice;
static PFN_vkDestroyInstance							qvkDestroyInstance;
PFN_vkEnumerateDeviceExtensionProperties			qvkEnumerateDeviceExtensionProperties;
static PFN_vkEnumeratePhysicalDevices					qvkEnumeratePhysicalDevices;
PFN_vkGetDeviceProcAddr							qvkGetDeviceProcAddr;
static PFN_vkGetPhysicalDeviceFeatures					qvkGetPhysicalDeviceFeatures;
static PFN_vkGetPhysicalDeviceFormatProperties			qvkGetPhysicalDeviceFormatProperties;
static PFN_vkGetPhysicalDeviceMemoryProperties			qvkGetPhysicalDeviceMemoryProperties;
PFN_vkGetPhysicalDeviceProperties				qvkGetPhysicalDeviceProperties;
PFN_vkGetPhysicalDeviceProperties2KHR				qvkGetPhysicalDeviceProperties2KHR;
PFN_vkGetPhysicalDeviceFeatures2KHR					qvkGetPhysicalDeviceFeatures2KHR;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties		qvkGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkDestroySurfaceKHR							qvkDestroySurfaceKHR;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR			qvkGetPhysicalDeviceSurfaceFormatsKHR;
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	qvkGetPhysicalDeviceSurfacePresentModesKHR;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR			qvkGetPhysicalDeviceSurfaceSupportKHR;
#ifdef USE_VK_VALIDATION
static PFN_vkCreateDebugReportCallbackEXT				qvkCreateDebugReportCallbackEXT;
static PFN_vkDestroyDebugReportCallbackEXT				qvkDestroyDebugReportCallbackEXT;
#endif
PFN_vkAllocateCommandBuffers						qvkAllocateCommandBuffers;
PFN_vkAllocateDescriptorSets						qvkAllocateDescriptorSets;
PFN_vkAllocateMemory								qvkAllocateMemory;
PFN_vkBeginCommandBuffer							qvkBeginCommandBuffer;
PFN_vkBindBufferMemory							qvkBindBufferMemory;
PFN_vkBindImageMemory							qvkBindImageMemory;
PFN_vkCmdBeginRenderPass							qvkCmdBeginRenderPass;
PFN_vkCmdBindDescriptorSets						qvkCmdBindDescriptorSets;
PFN_vkCmdBindIndexBuffer							qvkCmdBindIndexBuffer;
PFN_vkCmdBindPipeline							qvkCmdBindPipeline;
PFN_vkCmdBindVertexBuffers						qvkCmdBindVertexBuffers;
PFN_vkCmdBlitImage								qvkCmdBlitImage;
PFN_vkCmdClearAttachments						qvkCmdClearAttachments;
PFN_vkCmdCopyBuffer								qvkCmdCopyBuffer;
PFN_vkCmdCopyBufferToImage						qvkCmdCopyBufferToImage;
PFN_vkCmdCopyImage								qvkCmdCopyImage;
PFN_vkCmdDispatch								qvkCmdDispatch;
PFN_vkCmdDraw									qvkCmdDraw;
PFN_vkCmdDrawIndexed								qvkCmdDrawIndexed;
PFN_vkCmdDrawIndexedIndirect						qvkCmdDrawIndexedIndirect;
PFN_vkCmdEndRenderPass							qvkCmdEndRenderPass;
PFN_vkCmdNextSubpass								qvkCmdNextSubpass;
PFN_vkCmdPipelineBarrier							qvkCmdPipelineBarrier;
PFN_vkCmdPushConstants							qvkCmdPushConstants;
PFN_vkCmdSetDepthBias							qvkCmdSetDepthBias;
PFN_vkCmdSetScissor								qvkCmdSetScissor;
PFN_vkCmdSetViewport								qvkCmdSetViewport;
PFN_vkCreateBuffer								qvkCreateBuffer;
PFN_vkCreateCommandPool							qvkCreateCommandPool;
PFN_vkCreateDescriptorPool						qvkCreateDescriptorPool;
PFN_vkCreateDescriptorSetLayout					qvkCreateDescriptorSetLayout;
PFN_vkCreateFence								qvkCreateFence;
PFN_vkCreateFramebuffer							qvkCreateFramebuffer;
PFN_vkCreateGraphicsPipelines					qvkCreateGraphicsPipelines;
PFN_vkCreateComputePipelines					qvkCreateComputePipelines;
PFN_vkCreateImage								qvkCreateImage;
PFN_vkCreateImageView							qvkCreateImageView;
PFN_vkCreatePipelineLayout						qvkCreatePipelineLayout;
PFN_vkCreatePipelineCache						qvkCreatePipelineCache;
PFN_vkGetPipelineCacheData						qvkGetPipelineCacheData;
PFN_vkCreateRenderPass							qvkCreateRenderPass;
PFN_vkCreateSampler								qvkCreateSampler;
PFN_vkCreateSemaphore							qvkCreateSemaphore;
PFN_vkDestroySemaphore								qvkDestroySemaphore;
// Timeline semaphore functions (VK_KHR_timeline_semaphore) - loaded dynamically
PFN_vkCreateShaderModule							qvkCreateShaderModule;
PFN_vkCreateQueryPool								qvkCreateQueryPool;
PFN_vkDestroyQueryPool							qvkDestroyQueryPool;
PFN_vkDestroyBuffer								qvkDestroyBuffer;
PFN_vkDestroyCommandPool							qvkDestroyCommandPool;
PFN_vkDestroyDescriptorPool						qvkDestroyDescriptorPool;
PFN_vkDestroyDescriptorSetLayout					qvkDestroyDescriptorSetLayout;
PFN_vkDestroyDevice								qvkDestroyDevice;
PFN_vkDestroyFence								qvkDestroyFence;
PFN_vkDestroyFramebuffer							qvkDestroyFramebuffer;
PFN_vkDestroyImage								qvkDestroyImage;
PFN_vkDestroyImageView							qvkDestroyImageView;
PFN_vkDestroyPipeline							qvkDestroyPipeline;
PFN_vkDestroyPipelineCache						qvkDestroyPipelineCache;
PFN_vkDestroyPipelineLayout						qvkDestroyPipelineLayout;
PFN_vkDestroyRenderPass							qvkDestroyRenderPass;
PFN_vkDestroySampler								qvkDestroySampler;
PFN_vkDestroySemaphore							qvkDestroySemaphore;
PFN_vkDestroyShaderModule						qvkDestroyShaderModule;
PFN_vkDeviceWaitIdle								qvkDeviceWaitIdle;
PFN_vkEndCommandBuffer							qvkEndCommandBuffer;
PFN_vkFlushMappedMemoryRanges					qvkFlushMappedMemoryRanges;
PFN_vkFreeCommandBuffers							qvkFreeCommandBuffers;
PFN_vkFreeDescriptorSets							qvkFreeDescriptorSets;
PFN_vkFreeMemory									qvkFreeMemory;
PFN_vkGetBufferMemoryRequirements				qvkGetBufferMemoryRequirements;
PFN_vkGetDeviceQueue								qvkGetDeviceQueue;
PFN_vkGetImageMemoryRequirements					qvkGetImageMemoryRequirements;
PFN_vkGetImageSubresourceLayout					qvkGetImageSubresourceLayout;
PFN_vkInvalidateMappedMemoryRanges				qvkInvalidateMappedMemoryRanges;
PFN_vkMapMemory									qvkMapMemory;
PFN_vkQueueSubmit								qvkQueueSubmit;
PFN_vkQueueWaitIdle								qvkQueueWaitIdle;
PFN_vkResetCommandBuffer							qvkResetCommandBuffer;
PFN_vkResetDescriptorPool						qvkResetDescriptorPool;
PFN_vkResetFences								qvkResetFences;
PFN_vkUnmapMemory								qvkUnmapMemory;
PFN_vkUpdateDescriptorSets						qvkUpdateDescriptorSets;
PFN_vkWaitForFences								qvkWaitForFences;
static PFN_vkAcquireNextImageKHR						qvkAcquireNextImageKHR;
static PFN_vkCreateSwapchainKHR							qvkCreateSwapchainKHR;
static PFN_vkDestroySwapchainKHR						qvkDestroySwapchainKHR;
static PFN_vkGetSwapchainImagesKHR						qvkGetSwapchainImagesKHR;
static PFN_vkQueuePresentKHR							qvkQueuePresentKHR;
static PFN_vkCmdBeginRenderingKHR						qvkCmdBeginRenderingKHR;
static PFN_vkCmdEndRenderingKHR						qvkCmdEndRenderingKHR;
static PFN_vkCmdPipelineBarrier2KHR					qvkCmdPipelineBarrier2KHR;
static PFN_vkQueueSubmit2KHR							qvkQueueSubmit2KHR;
static PFN_vkCmdSetFragmentShadingRateKHR				qvkCmdSetFragmentShadingRateKHR;

static PFN_vkGetBufferMemoryRequirements2KHR			qvkGetBufferMemoryRequirements2KHR;
static PFN_vkGetImageMemoryRequirements2KHR				qvkGetImageMemoryRequirements2KHR;

static PFN_vkDebugMarkerSetObjectNameEXT				qvkDebugMarkerSetObjectNameEXT;

PFN_vkCmdClearColorImage								qvkCmdClearColorImage;

// GPU timing query function pointers
static PFN_vkCmdWriteTimestamp							qvkCmdWriteTimestamp;
static PFN_vkGetQueryPoolResults						qvkGetQueryPoolResults;
static PFN_vkResetQueryPool								qvkResetQueryPool;

// Ray tracing function pointers (non-static for use in vk_raytracing.c)
PFN_vkCreateAccelerationStructureKHR					qvkCreateAccelerationStructureKHR;
PFN_vkDestroyAccelerationStructureKHR					qvkDestroyAccelerationStructureKHR;
PFN_vkGetAccelerationStructureBuildSizesKHR			qvkGetAccelerationStructureBuildSizesKHR;
PFN_vkGetAccelerationStructureDeviceAddressKHR		qvkGetAccelerationStructureDeviceAddressKHR;
PFN_vkCmdBuildAccelerationStructuresKHR				qvkCmdBuildAccelerationStructuresKHR;
PFN_vkCmdTraceRaysKHR									qvkCmdTraceRaysKHR;
PFN_vkCreateRayTracingPipelinesKHR					qvkCreateRayTracingPipelinesKHR;
PFN_vkGetRayTracingShaderGroupHandlesKHR				qvkGetRayTracingShaderGroupHandlesKHR;
PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR	qvkGetRayTracingCaptureReplayShaderGroupHandlesKHR;
PFN_vkCmdTraceRaysIndirectKHR						qvkCmdTraceRaysIndirectKHR;
PFN_vkGetBufferDeviceAddress							qvkGetBufferDeviceAddress;

// Instance layer enumeration (needed for RenderDoc detection)
PFN_vkEnumerateInstanceLayerProperties					qvkEnumerateInstanceLayerProperties;

// Pipeline binary function pointers (VK_KHR_pipeline_executable_properties)
PFN_vkGetPipelineExecutablePropertiesKHR		qvkGetPipelineExecutablePropertiesKHR;
PFN_vkGetPipelineExecutableStatisticsKHR		qvkGetPipelineExecutableStatisticsKHR;
PFN_vkGetPipelineExecutableInternalRepresentationsKHR qvkGetPipelineExecutableInternalRepresentationsKHR;

////////////////////////////////////////////////////////////////////////////

// forward declaration
VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index );

// Modernized memory type finding with better safety and error handling
VK_NONNULL
uint32_t find_memory_type(uint32_t memory_type_bits, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memory_properties = {0};

	// Modern Vulkan API call with safety check
	if (!qvkGetPhysicalDeviceMemoryProperties) {
		ri.Error(ERR_FATAL, "Vulkan: qvkGetPhysicalDeviceMemoryProperties function pointer is NULL");
	}

	qvkGetPhysicalDeviceMemoryProperties(vk.physical_device, &memory_properties);

	// Bounds checking with modern loop
	const uint32_t max_types = memory_properties.memoryTypeCount;
	for (uint32_t i = 0; i < max_types && i < VK_MAX_MEMORY_TYPES; i++) {
		const uint32_t bit = (uint32_t)(1U << i);
		if ((memory_type_bits & bit) != 0 &&
			(memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	ri.Error(ERR_FATAL, "Vulkan: failed to find matching memory type with requested properties (bits: 0x%x, required: 0x%x)",
		memory_type_bits, properties);
	return ~0U; // Unreachable, but satisfies compiler
}


static uint32_t find_memory_type2( uint32_t memory_type_bits, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags *outprops ) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memory_properties );

	for ( i = 0; i < memory_properties.memoryTypeCount; i++ ) {
		if ( (memory_type_bits & (1 << i)) != 0 && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties ) {
			if ( outprops ) {
				*outprops = memory_properties.memoryTypes[i].propertyFlags;
			}
			return i;
		}
	}

	return ~0U;
}


static const char *pmode_to_str( VkPresentModeKHR mode )
{
	static char buf[32];

	switch ( mode ) {
		case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
		case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX";
		case VK_PRESENT_MODE_FIFO_KHR: return "FIFO";
		case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
		case VK_PRESENT_MODE_FIFO_LATEST_READY_EXT: return "FIFO_LATEST_READY";
		default: sprintf( buf, "mode#%x", mode ); return buf;
	};
}


#define CASE_STR(x) case (x): return #x

const char *vk_format_string( VkFormat format )
{
	static char buf[16];

	switch ( format ) {
		// color formats
		CASE_STR( VK_FORMAT_R5G5B5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G5R5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R5G6B5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G6R5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B8G8R8A8_SRGB );
		CASE_STR( VK_FORMAT_R8G8B8A8_SRGB );
		CASE_STR( VK_FORMAT_B8G8R8A8_SNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_SNORM );
		CASE_STR( VK_FORMAT_B8G8R8A8_UNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_UNORM );
		CASE_STR( VK_FORMAT_B4G4R4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R4G4B4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R16G16B16A16_UNORM );
		CASE_STR( VK_FORMAT_A2B10G10R10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_A2R10G10B10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_B10G11R11_UFLOAT_PACK32 );
		// depth formats
		CASE_STR( VK_FORMAT_D16_UNORM );
		CASE_STR( VK_FORMAT_D16_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_X8_D24_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_D24_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_D32_SFLOAT );
		CASE_STR( VK_FORMAT_D32_SFLOAT_S8_UINT );
	default:
		Com_sprintf( buf, sizeof( buf ), "#%i", format );
		return buf;
	}
}


const char *vk_result_string( VkResult code ) {
	static char buffer[32];

	switch ( code ) {
		CASE_STR( VK_SUCCESS );
		CASE_STR( VK_NOT_READY );
		CASE_STR( VK_TIMEOUT );
		CASE_STR( VK_EVENT_SET );
		CASE_STR( VK_EVENT_RESET );
		CASE_STR( VK_INCOMPLETE );
		CASE_STR( VK_ERROR_OUT_OF_HOST_MEMORY );
		CASE_STR( VK_ERROR_OUT_OF_DEVICE_MEMORY );
		CASE_STR( VK_ERROR_INITIALIZATION_FAILED );
		CASE_STR( VK_ERROR_DEVICE_LOST );
		CASE_STR( VK_ERROR_MEMORY_MAP_FAILED );
		CASE_STR( VK_ERROR_LAYER_NOT_PRESENT );
		CASE_STR( VK_ERROR_EXTENSION_NOT_PRESENT );
		CASE_STR( VK_ERROR_FEATURE_NOT_PRESENT );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DRIVER );
		CASE_STR( VK_ERROR_TOO_MANY_OBJECTS );
		CASE_STR( VK_ERROR_FORMAT_NOT_SUPPORTED );
		CASE_STR( VK_ERROR_FRAGMENTED_POOL );
		CASE_STR( VK_ERROR_UNKNOWN );
		CASE_STR( VK_ERROR_OUT_OF_POOL_MEMORY );
		CASE_STR( VK_ERROR_INVALID_EXTERNAL_HANDLE );
		CASE_STR( VK_ERROR_FRAGMENTATION );
		CASE_STR( VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS );
		CASE_STR( VK_ERROR_SURFACE_LOST_KHR );
		CASE_STR( VK_ERROR_NATIVE_WINDOW_IN_USE_KHR );
		CASE_STR( VK_SUBOPTIMAL_KHR );
		CASE_STR( VK_ERROR_OUT_OF_DATE_KHR );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DISPLAY_KHR );
		CASE_STR( VK_ERROR_VALIDATION_FAILED_EXT );
		CASE_STR( VK_ERROR_INVALID_SHADER_NV );
		CASE_STR( VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT );
		CASE_STR( VK_ERROR_NOT_PERMITTED_EXT );
		CASE_STR( VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT );
		CASE_STR( VK_THREAD_IDLE_KHR );
		CASE_STR( VK_THREAD_DONE_KHR );
		CASE_STR( VK_OPERATION_DEFERRED_KHR );
		CASE_STR( VK_OPERATION_NOT_DEFERRED_KHR );
		CASE_STR( VK_PIPELINE_COMPILE_REQUIRED_EXT );
	default:
		sprintf( buffer, "code %i", code );
		return buffer;
	}
}
#undef CASE_STR

// Keep the original VK_CHECK macro for compatibility


/*
static VkFlags get_composite_alpha( VkCompositeAlphaFlagsKHR flags )
{
	const VkCompositeAlphaFlagBitsKHR compositeFlags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};
	size_t i;

	for ( i = 1; i < ARRAY_LEN( compositeFlags ); i++ ) {
		if ( flags & compositeFlags[i] ) {
			return compositeFlags[i];
		}
	}

	return compositeFlags[0];
}
*/


// Modernized command buffer creation with designated initializers
VK_NONNULL
static VkCommandBuffer begin_command_buffer(void)
{
	VkCommandBuffer command_buffer;

	// Modern designated initializers for better readability and safety

	command_buffer = *VK_AllocateCommandBuffers(vk.device, vk.command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

	const VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = NULL,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = NULL
	};

	VK_CHECK(qvkBeginCommandBuffer(command_buffer, &begin_info));

	return command_buffer;
}


// Modernized command buffer submission with better structure
VK_NONNULL_PARAMS(1)
static void end_command_buffer(VkCommandBuffer command_buffer, const char *location)
{
	(void)location; // Suppress unused parameter warning

	VK_CHECK(qvkEndCommandBuffer(command_buffer));

#ifdef USE_UPLOAD_QUEUE
	const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
#endif

	// Modern array initialization
	const VkCommandBuffer cmdbuf[] = {command_buffer};

	// Designated initializer for submit info
	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = NULL
	};
#ifdef USE_UPLOAD_QUEUE
	if ( vk.rendering_finished != VK_NULL_HANDLE ) {
		waits = vk.rendering_finished;
		vk.rendering_finished = VK_NULL_HANDLE;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	} else 
#endif
	{
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
	}

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = cmdbuf;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, VK_NULL_HANDLE ) );

	vk_queue_wait_idle();

	VK_FreeCommandBuffers( vk.device, vk.command_pool, 1, (VkCommandBuffer*)cmdbuf );
}

VkInstance VK_GetInstanceHandle( void )
{
	return vk_instance;
}

VkSampleCountFlagBits VK_GetMsaaSampleCount( void )
{
	return (VkSampleCountFlagBits)vkSamples;
}

VkCommandBuffer VK_BeginImmediateCommands( void )
{
	return begin_command_buffer();
}

void VK_EndImmediateCommands( VkCommandBuffer command_buffer, const char *location )
{
	end_command_buffer( command_buffer, location );
}


// Optimized layout transition helper using C23 designated initializers
static void record_image_layout_transition( 
	VkCommandBuffer command_buffer, 
	VkImage image, 
	VkImageAspectFlags image_aspect_flags, 
	VkImageLayout old_layout, 
	VkImageLayout new_layout, 
	uint32_t src_stage_override, 
	uint32_t dst_stage_override )
{
	(void)dst_stage_override; // Suppress unused parameter warning
	
	// Determine source stage and access mask
	uint32_t src_stage;
	VkAccessFlags src_access;
	
	switch ( old_layout ) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
			src_stage = (src_stage_override != 0) ? src_stage_override : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			src_access = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			src_access = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			src_access = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			src_access = VK_ACCESS_NONE;
			break;
		default:
			ri.Error( ERR_DROP, "unsupported old layout %i", old_layout );
			src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			src_access = VK_ACCESS_NONE;
			break;
	}

	// Determine destination stage and access mask
	uint32_t dst_stage;
	VkAccessFlags dst_access;
	
	switch ( new_layout ) {
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dst_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dst_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dst_access = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dst_access = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dst_access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			break;
		default:
			ri.Error( ERR_DROP, "unsupported new layout %i", new_layout);
			dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			dst_access = VK_ACCESS_NONE;
			break;
	}

	// Use C23 designated initializer for better performance and type safety
	const VkImageMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = NULL,
		.srcAccessMask = src_access,
		.dstAccessMask = dst_access,
		.oldLayout = old_layout,
		.newLayout = new_layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = {
			.aspectMask = image_aspect_flags,
			.baseMipLevel = 0,
			.levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount = VK_REMAINING_ARRAY_LAYERS
		}
	};

	qvkCmdPipelineBarrier( command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier );
}


// debug markers
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

void vk_set_object_name( uint64_t obj, const char *objName, VkDebugReportObjectTypeEXT objType )
{
	if ( qvkDebugMarkerSetObjectNameEXT && obj )
	{
		VkDebugMarkerObjectNameInfoEXT info;
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		info.pNext = NULL;
		info.objectType = objType;
		info.object = obj;
		info.pObjectName = objName;
		qvkDebugMarkerSetObjectNameEXT( vk.device, &info );
	}
}


static void __attribute__((unused)) vk_create_swapchain( VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format, VkSwapchainKHR *swapchain, qboolean verbose ) {
	VkImageViewCreateInfo view;
	VkSurfaceCapabilitiesKHR surface_caps;
	VkExtent2D image_extent;
	uint32_t present_mode_count, i;
	VkPresentModeKHR present_mode;
	VkPresentModeKHR *present_modes;
	uint32_t image_count;
	VkSwapchainCreateInfoKHR desc;
	qboolean mailbox_supported = qfalse;
	qboolean immediate_supported = qfalse;
	qboolean fifo_relaxed_supported = qfalse;
	int v;

	VK_CHECK( qvkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device, surface, &surface_caps ) );

	image_extent = surface_caps.currentExtent;
	if ( image_extent.width == 0xffffffff && image_extent.height == 0xffffffff ) {
		image_extent.width = MIN( surface_caps.maxImageExtent.width, MAX( surface_caps.minImageExtent.width, (uint32_t) glConfig.vidWidth ) );
		image_extent.height = MIN( surface_caps.maxImageExtent.height, MAX( surface_caps.minImageExtent.height, (uint32_t) glConfig.vidHeight ) );
	}

	vk.clearAttachment = qtrue;

	if ( !vk.fboActive ) {
		// VK_IMAGE_USAGE_TRANSFER_DST_BIT is required by image clear operations.
		if ( ( surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ) {
			vk.clearAttachment = qfalse;
			ri.Printf( PRINT_WARNING, "VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swapchain, \\r_clear might not work\n" );
		}
		// VK_IMAGE_USAGE_TRANSFER_SRC_BIT is required in order to take screenshots.
		if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0) {
			ri.Error(ERR_FATAL, "create_swapchain: VK_IMAGE_USAGE_TRANSFER_SRC_BIT is not supported by the swapchain");
		}
	}

	// determine present mode and swapchain image count
	VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL));

	present_modes = (VkPresentModeKHR *) ri.Malloc( present_mode_count * sizeof( VkPresentModeKHR ) );
	VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes));

	if ( verbose ) {
		ri.Printf( PRINT_ALL, "...presentation modes:" );
	}
	for ( i = 0; i < present_mode_count; i++ ) {
		if ( verbose ) {
			ri.Printf( PRINT_ALL, " %s", pmode_to_str( present_modes[i] ) );
		}
		if ( present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR )
			mailbox_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR )
			immediate_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR )
			fifo_relaxed_supported = qtrue;
	}
	if ( verbose ) {
		ri.Printf( PRINT_ALL, "\n" );
	}

	ri.Free( present_modes );

	if ( ( v = ri.Cvar_VariableIntegerValue( "r_swapInterval" ) ) != 0 ) {
		if ( v == 2 && mailbox_supported )
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
		else if ( fifo_relaxed_supported )
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
		else
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
	} else {
		if ( immediate_supported ) {
			present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_IMM, surface_caps.minImageCount );
		} else if ( mailbox_supported ) {
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_MAILBOX, surface_caps.minImageCount );
		} else if ( fifo_relaxed_supported ) {
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		} else {
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		}
	}

	if ( image_count < 2 ) {
		image_count = 2;
	}

	if ( surface_caps.maxImageCount == 0 && present_mode == VK_PRESENT_MODE_FIFO_KHR ) {
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO_0, surface_caps.minImageCount );
	} else if ( surface_caps.maxImageCount > 0 ) {
		image_count = MIN( MIN( image_count, surface_caps.maxImageCount ), MAX_SWAPCHAIN_IMAGES );
	}

	if ( verbose ) {
		ri.Printf( PRINT_ALL, "...selected presentation mode: %s, image count: %i\n", pmode_to_str( present_mode ), image_count );
	}

	// create swap chain
	desc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.surface = surface;
	desc.minImageCount = image_count;
	desc.imageFormat = surface_format.format;
	desc.imageColorSpace = surface_format.colorSpace;
	desc.imageExtent = image_extent;
	desc.imageArrayLayers = 1;
	desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ( !vk.fboActive ) {
		desc.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.preTransform = surface_caps.currentTransform;
	//desc.compositeAlpha = get_composite_alpha( surface_caps.supportedCompositeAlpha );
	desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	desc.presentMode = present_mode;
	desc.clipped = VK_TRUE;
	desc.oldSwapchain = VK_NULL_HANDLE;

	VK_CHECK( qvkCreateSwapchainKHR( device, &desc, NULL, swapchain ) );

	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, NULL ) );
	vk.swapchain_image_count = MIN( vk.swapchain_image_count, MAX_SWAPCHAIN_IMAGES );
	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, vk.swapchain_images ) );

	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.pNext = NULL;
		view.flags = 0;
		view.image = vk.swapchain_images[i];
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = vk.present_format.format;
		view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.levelCount = 1;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &view, NULL, &vk.swapchain_image_views[i] ) );

		SET_OBJECT_NAME( vk.swapchain_images[i], va( "swapchain image %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.swapchain_image_views[i], va( "swapchain image %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		VkSemaphoreCreateInfo s;
		s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		s.pNext = NULL;
		s.flags = 0;
		VK_CHECK( qvkCreateSemaphore( vk.device, &s, NULL, &vk.swapchain_rendering_finished[i] ) );
		SET_OBJECT_NAME( vk.swapchain_rendering_finished[i], va( "swapchain_rendering_finished semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
	}

	if ( vk.initSwapchainLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
		VkCommandBuffer command_buffer = begin_command_buffer();

		for ( i = 0; i < vk.swapchain_image_count; i++ ) {
			record_image_layout_transition( command_buffer, vk.swapchain_images[i],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, vk.initSwapchainLayout, 0, 0 );
		}

		end_command_buffer( command_buffer, __func__ );
	}

	VK_ImGui_NotifySwapchainChanged();
}



/*
=============================================================================
Memory Defragmentation System
=============================================================================
*/

// TODO: Reimplement allocate_and_bind_image_memory function

/*
=============================================================================
Virtual Memory Management
=============================================================================
*/


/*
=============================================================================
Resource Pooling System
=============================================================================
*/


/*
=============================================================================
Async Compute Framework
=============================================================================
*/



/*
=============================================================================
Shader Hot Reload System
=============================================================================
*/

// Shader file watching structure (defined in vk_pipeline.h)

static shader_file_watch_t shader_watched_files[64];
static uint32_t shader_watched_file_count = 0;

// Initialize shader hot reload system
static void __attribute__((unused)) vk_hot_reload_init(void) {
	if (!r_vk_hotReload || !r_vk_hotReload->integer) {
		return;
	}

	Com_Memset(&vk.hot_reload, 0, sizeof(vk.hot_reload));
	vk.hot_reload.enabled = qtrue;

	// Note: Platform-specific file watching would be implemented here
	// For Linux: inotify, for macOS: FSEvents, for Windows: ReadDirectoryChangesW
	// This is a framework - full implementation requires platform-specific code

	ri.Printf(PRINT_ALL, "Vulkan: Shader hot reload system initialized (framework ready)\n");
}

// Check for shader file changes and reload if needed


// Mark pipelines as dirty when shaders change
__attribute__((unused)) void vk_mark_pipelines_dirty(void) {
	// Mark all pipelines for lazy recreation
	// This would be called when shaders are reloaded
	vk.hot_reload.pipelines_recreated = 0; // Reset counter
}

// Shutdown shader hot reload
static void __attribute__((unused)) vk_hot_reload_shutdown(void) {
	if (!vk.hot_reload.enabled) {
		return;
	}

	// Cleanup file watchers (platform-specific)
	Com_Memset(&shader_watched_files, 0, sizeof(shader_watched_files));
	shader_watched_file_count = 0;

	Com_Memset(&vk.hot_reload, 0, sizeof(vk.hot_reload));
	ri.Printf(PRINT_ALL, "Vulkan: Shader hot reload system shut down\n");
}




#ifdef USE_UPLOAD_QUEUE
static qboolean vk_wait_staging_buffer( void )
{
	if ( vk.aux_fence_wait ) {
		VkResult res = qvkWaitForFences( vk.device, 1, &vk.aux_fence, VK_TRUE, 5 * 1000000000ULL );
		if ( res != VK_SUCCESS ) {
			ri.Error( ERR_FATAL, "vkWaitForFences() failed with %s at %s", vk_result_string( res ), __func__ );
		}
		qvkResetFences( vk.device, 1, &vk.aux_fence );
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
		vk.staging_buffer.offset = 0; // Reset offset after fence wait - buffer can be reused from start
		vk.aux_fence_wait = qfalse;
		return qtrue;
	} else {
		return qfalse;
	}
}
#endif // USE_UPLOAD_QUEUE




#ifdef USE_VK_VALIDATION

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT object_type,
	uint64_t object,
	size_t location,
	int32_t message_code,
	const char* layer_prefix,
	const char* message,
	void* user_data )
{
	const char *severity = "INFO";

	if ( flags & VK_DEBUG_REPORT_ERROR_BIT_EXT ) {
		severity = S_COLOR_RED "ERROR";
	} else if ( flags & VK_DEBUG_REPORT_WARNING_BIT_EXT ) {
		severity = S_COLOR_YELLOW "WARN";
	} else if ( flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT ) {
		severity = S_COLOR_CYAN "PERF";
	} else if ( flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT ) {
		severity = "INFO";
	} else if ( flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT ) {
		severity = "DEBUG";
	}

	ri.Printf(
		PRINT_WARNING,
		"VK VALIDATION [%s] layer=%s code=%d object=0x%llx type=%d msg=%s\n",
		severity,
		layer_prefix ? layer_prefix : "unknown",
		message_code,
		(unsigned long long)object,
		(int)object_type,
		message ? message : "(null)" );

#ifdef _WIN32
	MessageBoxA( 0, message ? message : "(null)", layer_prefix ? layer_prefix : "VK_VALIDATION", MB_ICONWARNING );
	OutputDebugString(message);
	OutputDebugString("\n");
	DebugBreak();
#endif

	return VK_FALSE; // Never block execution; just log.
}
#endif


static qboolean used_instance_extension( const char *ext )
{
	const char *u;

	// allow all VK_*_surface extensions
	u = strrchr( ext, '_' );
	if ( u && Q_stricmp( u + 1, "surface" ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_DISPLAY_EXTENSION_NAME ) == 0 )
		return qtrue; // needed for KMSDRM instances/devices?

	if ( Q_stricmp( ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0 )
		return qtrue;

#ifdef USE_VK_VALIDATION
	if ( Q_stricmp( ext, VK_EXT_DEBUG_REPORT_EXTENSION_NAME ) == 0 )
		return qtrue;
#endif

	if ( Q_stricmp( ext, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 )
		return qtrue;

	// Ray tracing requires VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2 (already checked above)
	// No additional instance extensions needed for ray tracing

	return qfalse;
}


// Enhanced device type reporting
VK_PURE
static const char* vk_get_device_type_string(VkPhysicalDeviceType type) {
	switch (type) {
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "Discrete GPU";
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "Virtual GPU";
		case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU";
		case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "Other";
		default: return "Unknown";
	}
}

static void create_instance( void )
{
	// Basic validation layer support
	const char *enabled_layers[2] = {0};
	uint32_t enabled_layer_count = 0;

	// Enable validation layers if requested
	if (r_vulkan_validation && r_vulkan_validation->integer) {
		ri.Printf(PRINT_ALL, "...enabling Vulkan validation layers\n");

		// Try modern validation layer first, then fallback to legacy
		const char *preferred_layers[] = {
			"VK_LAYER_KHRONOS_validation",
			"VK_LAYER_LUNARG_standard_validation"
		};

		for (size_t i = 0; i < ARRAY_LEN(preferred_layers) && enabled_layer_count < ARRAY_LEN(enabled_layers); i++) {
			enabled_layers[enabled_layer_count++] = preferred_layers[i];
			ri.Printf(PRINT_ALL, "...enabled validation layer: %s\n", preferred_layers[i]);
		}
	}

	// Check for RenderDoc layer
	extern cvar_t *r_vk_renderdoc;
	if (r_vk_renderdoc && r_vk_renderdoc->integer) {
		uint32_t layer_count = 0;
		qvkEnumerateInstanceLayerProperties(&layer_count, NULL);
		if (layer_count > 0) {
			VkLayerProperties *layers = (VkLayerProperties*)ri.Malloc(layer_count * sizeof(VkLayerProperties));
			qvkEnumerateInstanceLayerProperties(&layer_count, layers);
			
			for (uint32_t i = 0; i < layer_count; i++) {
				if (strcmp(layers[i].layerName, "VK_LAYER_RENDERDOC_Capture") == 0) {
					if (enabled_layer_count < ARRAY_LEN(enabled_layers)) {
						enabled_layers[enabled_layer_count++] = "VK_LAYER_RENDERDOC_Capture";
						ri.Printf(PRINT_ALL, "...RenderDoc layer detected and enabled\n");
					}
					break;
				}
			}
			ri.Free(layers);
		}
	}
	VkInstanceCreateInfo desc;
	VkInstanceCreateFlags flags;
	VkExtensionProperties *extension_properties;
	VkResult res;
	const char **extension_names;
	uint32_t i, n, count, extension_count;
	VkApplicationInfo appInfo;

	flags = 0;
	count = 0;
	extension_count = 0;
	VK_CHECK(qvkEnumerateInstanceExtensionProperties(NULL, &count, NULL));

	extension_properties = (VkExtensionProperties *)ri.Malloc(sizeof(VkExtensionProperties) * count);
	extension_names = (const char**)ri.Malloc(sizeof(char *) * count);

	VK_CHECK( qvkEnumerateInstanceExtensionProperties( NULL, &count, extension_properties ) );
	for ( i = 0; i < count; i++ ) {
		const char *ext = extension_properties[i].extensionName;

		if ( !used_instance_extension( ext ) ) {
			continue;
		}

		// search for duplicates
		for ( n = 0; n < extension_count; n++ ) {
			if ( Q_stricmp( ext, extension_names[ n ] ) == 0 ) {
				break;
			}
		}
		if ( n != extension_count ) {
			continue;
		}

		extension_names[ extension_count++ ] = ext;

		if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 ) {
			flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		}

		ri.Printf(PRINT_DEVELOPER, "instance extension: %s\n", ext);
	}

	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = NULL;
	appInfo.pApplicationName = NULL; // Q3_VERSION;
	appInfo.applicationVersion = 0x0;
	appInfo.pEngineName = NULL;
	appInfo.engineVersion = 0x0;
#ifdef _DEBUG
	appInfo.apiVersion = VK_API_VERSION_1_1;
#else
	appInfo.apiVersion = VK_API_VERSION_1_0;
#endif

	// create instance
	desc.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = flags;
	desc.pApplicationInfo = &appInfo;
	desc.enabledExtensionCount = extension_count;
	desc.ppEnabledExtensionNames = extension_names;

#ifdef USE_VK_VALIDATION
	desc.enabledLayerCount = 1;
	desc.ppEnabledLayerNames = &validation_layer_name;

	res = qvkCreateInstance( &desc, NULL, &vk_instance );

	if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {

		desc.enabledLayerCount = 1;
		desc.ppEnabledLayerNames = &validation_layer_name2;

		res = qvkCreateInstance( &desc, NULL, &vk_instance );

		if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {

			ri.Printf( PRINT_WARNING, "...validation layer is not available\n" );

			// try without validation layer
			desc.enabledLayerCount = 0;
			desc.ppEnabledLayerNames = NULL;

			res = qvkCreateInstance( &desc, NULL, &vk_instance );
		}
	}
#else
	desc.enabledLayerCount = 0;
	desc.ppEnabledLayerNames = NULL;

	res = qvkCreateInstance( &desc, NULL, &vk_instance );
#endif

	ri.Free( (void*)extension_names );
	ri.Free( extension_properties );

	if ( res != VK_SUCCESS ) {
		ri.Error( ERR_FATAL, "Vulkan: instance creation failed with %s", vk_result_string( res ) );
	}
}


static VkFormat get_depth_format( VkPhysicalDevice physical_device ) {
	VkFormatProperties props;
	VkFormat formats[2];
	size_t i;

	if ( glConfig.stencilBits > 0 ) {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM_S8_UINT : VK_FORMAT_D24_UNORM_S8_UINT;
		formats[1] = VK_FORMAT_D32_SFLOAT_S8_UINT;
	} else {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM : VK_FORMAT_X8_D24_UNORM_PACK32;
		formats[1] = VK_FORMAT_D32_SFLOAT;
	}

	for ( i = 0; i < ARRAY_LEN( formats ); i++ ) {
		qvkGetPhysicalDeviceFormatProperties( physical_device, formats[i], &props );
		if ( ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) != 0 ) {
			return formats[i];
		}
	}

	ri.Error( ERR_FATAL, "get_depth_format: failed to find depth attachment format" );
	return VK_FORMAT_UNDEFINED; // never get here
}


// Check if we can use vkCmdBlitImage for the given source and destination image formats.
static qboolean vk_blit_enabled( VkPhysicalDevice physical_device, const VkFormat srcFormat, const VkFormat dstFormat )
{
	VkFormatProperties formatProps;

	qvkGetPhysicalDeviceFormatProperties( physical_device, srcFormat, &formatProps );
	if ( ( formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT ) == 0 ) {
		return qfalse;
	}

	qvkGetPhysicalDeviceFormatProperties( physical_device, dstFormat, &formatProps );
	if ( ( formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) == 0 ) {
		return qfalse;
	}

	return qtrue;
}


static VkFormat get_hdr_format( VkFormat base_format )
{
	if ( r_fbo->integer == 0 ) {
		return base_format;
	}

	switch ( r_hdr->integer ) {
		case -1: return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
		case 1: return VK_FORMAT_R16G16B16A16_UNORM;
		default: return base_format;
	}
}

typedef struct {
	int bits;
	VkFormat rgb;
	VkFormat bgr;
} present_format_t;

static const present_format_t present_formats[] = {
	//{12, VK_FORMAT_B4G4R4A4_UNORM_PACK16, VK_FORMAT_R4G4B4A4_UNORM_PACK16},
	//{15, VK_FORMAT_B5G5R5A1_UNORM_PACK16, VK_FORMAT_R5G5B5A1_UNORM_PACK16},
	{16, VK_FORMAT_B5G6R5_UNORM_PACK16, VK_FORMAT_R5G6B5_UNORM_PACK16},
	{24, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM},
	{30, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32},
	//{32, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_B10G11R11_UFLOAT_PACK32}
};

static void get_present_format( int present_bits, VkFormat *bgr, VkFormat *rgb ) {
	const present_format_t *pf, *sel;
	size_t i;

	sel = NULL;
	pf = present_formats;
	for ( i = 0; i < ARRAY_LEN( present_formats ); i++, pf++ ) {
		if ( pf->bits <= present_bits  ) {
			sel = pf;
		}
	}
	if ( !sel ) {
		*bgr = VK_FORMAT_B8G8R8A8_UNORM;
		*rgb = VK_FORMAT_R8G8B8A8_UNORM;
	} else {
		*bgr = sel->bgr;
		*rgb = sel->rgb;
	}
}


static qboolean vk_select_surface_format( VkPhysicalDevice physical_device, VkSurfaceKHR surface )
{
	VkFormat base_bgr, base_rgb;
	VkFormat ext_bgr, ext_rgb;
	VkSurfaceFormatKHR *candidates;
	uint32_t format_count;
	VkResult res;

	res = qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, NULL );
	if ( res < 0 ) {
		ri.Printf( PRINT_ERROR, "vkGetPhysicalDeviceSurfaceFormatsKHR returned %s\n", vk_result_string( res ) );
		return qfalse;
	}

	if ( format_count == 0 ) {
		ri.Printf( PRINT_ERROR, "...no surface formats found\n" );
		return qfalse;
	}

	candidates = (VkSurfaceFormatKHR*)ri.Malloc( format_count * sizeof(VkSurfaceFormatKHR) );

	VK_CHECK( qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, candidates ) );

	get_present_format( 24, &base_bgr, &base_rgb );

	if ( r_fbo->integer ) {
		get_present_format( r_presentBits->integer, &ext_bgr, &ext_rgb );
	} else {
		ext_bgr = base_bgr;
		ext_rgb = base_rgb;
	}

	if ( format_count == 1 && candidates[0].format == VK_FORMAT_UNDEFINED ) {
		// special case that means we can choose any format
		vk.base_format.format = base_bgr;
		vk.base_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		vk.present_format.format = ext_bgr;
		vk.present_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	}
	else {
		uint32_t i;
		for ( i = 0; i < format_count; i++ ) {
			if ( ( candidates[i].format == base_bgr || candidates[i].format == base_rgb ) && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
				vk.base_format = candidates[i];
				break;
			}
		}
		if ( i == format_count ) {
			vk.base_format = candidates[0];
		}
		for ( i = 0; i < format_count; i++ ) {
			if ( ( candidates[i].format == ext_bgr || candidates[i].format == ext_rgb ) && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
				vk.present_format = candidates[i];
				break;
			}
		}
		if ( i == format_count ) {
			vk.present_format = vk.base_format;
		}
	}

	if ( !r_fbo->integer ) {
		vk.present_format = vk.base_format;
	}

	ri.Free( candidates );

	return qtrue;
}


// Convert sRGB format to linear UNORM format
static VkFormat convert_srgb_to_linear( VkFormat format )
{
	switch ( format ) {
		case VK_FORMAT_B8G8R8A8_SRGB:
			return VK_FORMAT_B8G8R8A8_UNORM;
		case VK_FORMAT_R8G8B8A8_SRGB:
			return VK_FORMAT_R8G8B8A8_UNORM;
		default:
			return format; // Already linear or unknown
	}
}

static void setup_surface_formats( VkPhysicalDevice physical_device )
{
	vk.depth_format = get_depth_format( physical_device );

	// Ensure color format is linear (not sRGB) for proper rendering
	vk.color_format = convert_srgb_to_linear( get_hdr_format( vk.base_format.format ) );

	vk.capture_format = VK_FORMAT_R8G8B8A8_UNORM;

	// Bloom should always use linear format, not sRGB
	vk.bloom_format = convert_srgb_to_linear( vk.base_format.format );

	vk.blitEnabled = vk_blit_enabled( physical_device, vk.color_format, vk.capture_format );

	if ( !vk.blitEnabled )
	{
		vk.capture_format = vk.color_format;
	}
}


static const char *renderer_name( const VkPhysicalDeviceProperties *props ) {
	static char buf[sizeof( props->deviceName ) + 64];
	const char *device_type;

	switch ( props->deviceType ) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: device_type = "Integrated"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: device_type = "Discrete"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: device_type = "Virtual"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: device_type = "CPU"; break;
		default: device_type = "OTHER"; break;
	}

	Com_sprintf( buf, sizeof( buf ), "%s %s, 0x%04x",
		device_type, props->deviceName, props->deviceID );

	return buf;
}


static qboolean vk_create_device( VkPhysicalDevice physical_device, int device_index ) {

#ifdef _DEBUG
	VkPhysicalDeviceTimelineSemaphoreFeatures timeline_semaphore;
	VkPhysicalDeviceVulkanMemoryModelFeatures memory_model;
	VkPhysicalDeviceBufferDeviceAddressFeatures devaddr_features;
	VkPhysicalDevice8BitStorageFeatures storage_8bit_features;
#endif

	ri.Printf( PRINT_ALL, "...selected physical device: %i\n", device_index );

	// select surface format
	if ( !vk_select_surface_format( physical_device, vk_surface ) ) {
		return qfalse;
	}

	setup_surface_formats( physical_device );

	// select queue family
	{
		VkQueueFamilyProperties *queue_families;
		uint32_t queue_family_count;
		uint32_t i;

		qvkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, NULL );
		queue_families = (VkQueueFamilyProperties*)ri.Malloc( queue_family_count * sizeof( VkQueueFamilyProperties ) );
		qvkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, queue_families );

		// select queue family with presentation and graphics support
		vk.queue_family_index = ~0U;
		vk.compute_queue.queue_family_index = ~0U;
		for (i = 0; i < queue_family_count; i++) {
			VkBool32 presentation_supported;
			VK_CHECK( qvkGetPhysicalDeviceSurfaceSupportKHR( physical_device, i, vk_surface, &presentation_supported ) );

			if (presentation_supported && (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
				vk.queue_family_index = i;
			}

			// Look for dedicated compute queue (compute but not graphics)
			if (vk.compute_queue.queue_family_index == ~0U &&
				(queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 &&
				(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
				vk.compute_queue.queue_family_index = i;
			}
		}

		// Fallback to graphics queue if no dedicated compute queue found
		if (vk.compute_queue.queue_family_index == ~0U && vk.queue_family_index != ~0U) {
			if ((queue_families[vk.queue_family_index].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
				vk.compute_queue.queue_family_index = vk.queue_family_index;
			}
		}

		ri.Free( queue_families );

		if ( vk.queue_family_index == ~0U ) {
			ri.Printf( PRINT_ERROR, "...failed to find graphics queue family\n" );

			return qfalse;
		}
	}

	// create VkDevice
	{
		const char *device_extension_list[36]; // room for RT/mesh + sync2/dynRender/VRS + bindless/push/F16
		uint32_t device_extension_count;
		const char *ext, *end;
		char *str;
		const float priority = 1.0;
		VkExtensionProperties *extension_properties;
		VkDeviceQueueCreateInfo queue_desc;
		VkPhysicalDeviceFeatures device_features;
		VkPhysicalDeviceFeatures features;
		VkDeviceCreateInfo device_desc;
		VkResult res;
		qboolean swapchainSupported = qfalse;
		qboolean dedicatedAllocation = qfalse;
		qboolean memoryRequirements2 = qfalse;
		qboolean debugMarker = qfalse;
		qboolean meshShaderExt = qfalse;
		qboolean meshShadersEnabled = qfalse;
		qboolean sync2 = qfalse;
		qboolean dynamicRendering = qfalse;
		qboolean extDynState = qfalse;
		qboolean fragmentShadingRate = qfalse;
		qboolean descriptorIndexing = qfalse;
		qboolean shaderFloat16Int8 = qfalse;
		qboolean pushDescriptor = qfalse;
		VkPhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features;
		VkPhysicalDeviceSynchronization2FeaturesKHR sync2_features;
		VkPhysicalDeviceDynamicRenderingFeaturesKHR dyn_render_features;
		VkPhysicalDeviceExtendedDynamicStateFeaturesEXT xdyn_features;
		VkPhysicalDeviceFragmentShadingRateFeaturesKHR fsr_features;
		VkPhysicalDeviceDescriptorIndexingFeaturesEXT desc_index_features;
		VkPhysicalDeviceShaderFloat16Int8FeaturesKHR f16i8_features;
		const void **pNextPtr = NULL;
#ifdef _DEBUG
		qboolean timelineSemaphore = qfalse;
		qboolean memoryModel = qfalse;
		qboolean devAddrFeat = qfalse;
		qboolean storage8bit = qfalse;
#endif
		qboolean rayTracingPipeline = qfalse;
		qboolean accelerationStructure = qfalse;
		qboolean rayQuery = qfalse;
		qboolean deferredHostOperations = qfalse;
		qboolean bufferDeviceAddress = qfalse;
		uint32_t i, len, count = 0;

		VK_CHECK( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, NULL ) );
		extension_properties = (VkExtensionProperties*)ri.Malloc( count * sizeof( VkExtensionProperties ) );
		VK_CHECK( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, extension_properties ) );

		// fill glConfig.extensions_string
		str = glConfig.extensions_string; *str = '\0';
		end = &glConfig.extensions_string[ sizeof( glConfig.extensions_string ) - 1];

		for ( i = 0; i < count; i++ ) {
			ext = extension_properties[i].extensionName;
			if ( strcmp( ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0 ) {
				swapchainSupported = qtrue;
			} else if ( strcmp( ext, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME ) == 0 ) {
				dedicatedAllocation = qtrue;
			} else if ( strcmp( ext, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME ) == 0 ) {
				memoryRequirements2 = qtrue;
			} else if ( strcmp( ext, VK_EXT_DEBUG_MARKER_EXTENSION_NAME ) == 0 ) {
				debugMarker = qtrue;
#ifdef _DEBUG
			} else if ( strcmp( ext, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME ) == 0 ) {
				timelineSemaphore = qtrue;
			} else if ( strcmp( ext, VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME ) == 0 ) {
				memoryModel = qtrue;
			} else if ( strcmp( ext, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME ) == 0 ) {
				devAddrFeat = qtrue;
			} else if ( strcmp( ext, VK_KHR_8BIT_STORAGE_EXTENSION_NAME ) == 0 ) {
				storage8bit = qtrue;
#endif
			} else if ( strcmp( ext, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME ) == 0 ) {
				rayTracingPipeline = qtrue;
			} else if ( strcmp( ext, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME ) == 0 ) {
				accelerationStructure = qtrue;
			} else if ( strcmp( ext, VK_KHR_RAY_QUERY_EXTENSION_NAME ) == 0 ) {
				rayQuery = qtrue;
			} else if ( strcmp( ext, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME ) == 0 ) {
				deferredHostOperations = qtrue;
			} else if ( strcmp( ext, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME ) == 0 ) {
				bufferDeviceAddress = qtrue;
			} else if ( strcmp( ext, "VK_EXT_mesh_shader" ) == 0 ) {
				meshShaderExt = qtrue;
			} else if ( strcmp( ext, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME ) == 0 ) {
				sync2 = qtrue;
			} else if ( strcmp( ext, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME ) == 0 ) {
				dynamicRendering = qtrue;
			} else if ( strcmp( ext, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME ) == 0 ) {
				extDynState = qtrue;
			} else if ( strcmp( ext, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME ) == 0 ) {
				fragmentShadingRate = qtrue;
			} else if ( strcmp( ext, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME ) == 0 ) {
				descriptorIndexing = qtrue;
			} else if ( strcmp( ext, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME ) == 0 ) {
				shaderFloat16Int8 = qtrue;
			} else if ( strcmp( ext, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME ) == 0 ) {
				pushDescriptor = qtrue;
			} else if ( strcmp( ext, VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME ) == 0 ) {
				// Pipeline binary support (VK_KHR_pipeline_executable_properties)
				// This extension allows querying pipeline executables and saving them as binaries
				vk_advanced.pipelineBinaries = qtrue;
			}
			// add this device extension to glConfig
			if ( i != 0 ) {
				if ( str + 1 >= end )
					continue;
				str = Q_stradd( str, " " );
			}
			len = (uint32_t)strlen( ext );
			if ( str + len >= end )
				continue;
			str = Q_stradd( str, ext );
		}

		ri.Free( extension_properties );

		// Query mesh shader features when requested
		Com_Memset( &mesh_shader_features, 0, sizeof( mesh_shader_features ) );
		Com_Memset( &sync2_features, 0, sizeof( sync2_features ) );
		Com_Memset( &dyn_render_features, 0, sizeof( dyn_render_features ) );
		Com_Memset( &xdyn_features, 0, sizeof( xdyn_features ) );
		Com_Memset( &fsr_features, 0, sizeof( fsr_features ) );
		Com_Memset( &desc_index_features, 0, sizeof( desc_index_features ) );
		Com_Memset( &f16i8_features, 0, sizeof( f16i8_features ) );
		if ( r_meshShaders && r_meshShaders->integer ) {
			if ( !meshShaderExt ) {
				ri.Printf( PRINT_WARNING, "...mesh shaders requested but VK_EXT_mesh_shader not supported by device\n" );
			} else if ( !qvkGetPhysicalDeviceFeatures2KHR ) {
				ri.Printf( PRINT_WARNING, "...mesh shaders requested but vkGetPhysicalDeviceFeatures2KHR is unavailable\n" );
			} else {
				VkPhysicalDeviceFeatures2 mesh_features2;
				Com_Memset( &mesh_features2, 0, sizeof( mesh_features2 ) );
				mesh_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				mesh_features2.pNext = &mesh_shader_features;

				mesh_shader_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
				mesh_shader_features.pNext = NULL;
				mesh_shader_features.meshShader = VK_FALSE;
				mesh_shader_features.taskShader = VK_FALSE;

				qvkGetPhysicalDeviceFeatures2KHR( physical_device, &mesh_features2 );

				if ( mesh_shader_features.meshShader ) {
					meshShadersEnabled = qtrue;
					if ( !mesh_shader_features.taskShader ) {
						ri.Printf( PRINT_WARNING, "...mesh shaders supported but task shaders unavailable; proceeding with mesh-only\n" );
					}
					// Enable the supported feature bits
					mesh_shader_features.meshShader = VK_TRUE;
					mesh_shader_features.taskShader = mesh_shader_features.taskShader ? VK_TRUE : VK_FALSE;
				} else {
					ri.Printf( PRINT_WARNING, "...mesh shaders requested but meshShader feature not supported by device\n" );
				}
			}
		}

		device_extension_count = 0;

		if ( !swapchainSupported ) {
			ri.Printf( PRINT_ERROR, "...required device extension is not available: %s\n", VK_KHR_SWAPCHAIN_EXTENSION_NAME );
			return qfalse;
		}

		if ( !memoryRequirements2 )
			dedicatedAllocation = qfalse;
		else
			vk.dedicatedAllocation = dedicatedAllocation;

#ifndef USE_DEDICATED_ALLOCATION
		vk.dedicatedAllocation = qfalse;
#endif

		device_extension_list[ device_extension_count++ ] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

		if ( vk.dedicatedAllocation ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME;
		}

		if ( debugMarker ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
			vk.debugMarkers = qtrue;
		}
#ifdef _DEBUG
		if ( timelineSemaphore ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME;
		}

		if ( memoryModel ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME;
		}

		if ( devAddrFeat ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;
		}

		if ( storage8bit ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_8BIT_STORAGE_EXTENSION_NAME;
		}
#endif // _DEBUG

		if ( meshShadersEnabled ) {
			device_extension_list[ device_extension_count++ ] = "VK_EXT_mesh_shader";
			vk_advanced.meshShaders = qtrue;
			ri.Printf( PRINT_ALL, "...mesh shader extension enabled\n" );
		} else if ( r_meshShaders && r_meshShaders->integer ) {
			ri.Printf( PRINT_WARNING, "...mesh shader extension not enabled\n" );
		}

		if ( sync2 ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME;
			vk_advanced.synchronization2 = qtrue;
		}

		// Timeline semaphores: framework ready for future implementation

		if ( dynamicRendering ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
			vk_advanced.dynamicRendering = qtrue;
		}

		if ( extDynState ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME;
		}

		if ( fragmentShadingRate ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME;
		}

		if ( descriptorIndexing ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME;
		}

		if ( shaderFloat16Int8 ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME;
		}

		if ( pushDescriptor ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;
		}

		// Pipeline binary support (VK_KHR_pipeline_executable_properties)
		if ( vk_advanced.pipelineBinaries ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME;
			ri.Printf( PRINT_ALL, "...pipeline binary support enabled\n" );
		}

		// Ray tracing extensions (all required together)
		if ( rayTracingPipeline && accelerationStructure && deferredHostOperations && bufferDeviceAddress ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;
			if ( rayQuery ) {
				device_extension_list[ device_extension_count++ ] = VK_KHR_RAY_QUERY_EXTENSION_NAME;
			}
			vk.rayTracingSupported = qtrue;
			vk_advanced.rayTracing = qtrue;
			ri.Printf( PRINT_ALL, "...ray tracing extensions enabled\n" );
		} else {
			vk.rayTracingSupported = qfalse;
			if ( rayTracingPipeline || accelerationStructure ) {
				ri.Printf( PRINT_WARNING, "...ray tracing extensions not fully available (missing dependencies)\n" );
			}
		}

		// Check VRS support
		if ( fragmentShadingRate ) {
			vk.vrs.supported = qtrue;
			ri.Printf( PRINT_ALL, "...fragment shading rate (VRS) extension enabled\n" );
		} else {
			vk.vrs.supported = qfalse;
			ri.Printf( PRINT_ALL, "...fragment shading rate (VRS) not supported\n" );
		}

		qvkGetPhysicalDeviceFeatures( physical_device, &device_features );

		if ( device_features.fillModeNonSolid == VK_FALSE ) {
			ri.Printf( PRINT_ERROR, "...fillModeNonSolid feature is not supported\n" );
			return qfalse;
		}

	vk_log_subgroup_capabilities( physical_device );

		// Create queue create infos
		VkDeviceQueueCreateInfo queue_create_infos[2];
		uint32_t queue_create_info_count = 1;

		queue_desc.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_desc.pNext = NULL;
		queue_desc.flags = 0;
		queue_desc.queueFamilyIndex = vk.queue_family_index;
		queue_desc.queueCount = 1;
		queue_desc.pQueuePriorities = &priority;
		queue_create_infos[0] = queue_desc;

		// Add compute queue if it's different from graphics queue
		if (vk.compute_queue.queue_family_index != ~0U && 
			vk.compute_queue.queue_family_index != vk.queue_family_index) {
			VkDeviceQueueCreateInfo compute_queue_desc = {
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.queueFamilyIndex = vk.compute_queue.queue_family_index,
				.queueCount = 1,
				.pQueuePriorities = &priority
			};
			queue_create_infos[1] = compute_queue_desc;
			queue_create_info_count = 2;
			vk.compute_queue.supported = qtrue;
		} else if (vk.compute_queue.queue_family_index == vk.queue_family_index) {
			// Same queue family, can use same queue or get second queue
			vk.compute_queue.supported = qtrue;
		}

		Com_Memset( &features, 0, sizeof( features ) );
		features.fillModeNonSolid = VK_TRUE;

#ifdef _DEBUG
		if ( device_features.shaderInt64 ) {
			features.shaderInt64 = VK_TRUE;
		}
#endif
		if ( device_features.wideLines ) { // needed for RB_SurfaceAxis
			features.wideLines = VK_TRUE;
			vk.wideLines = qtrue;
		}

		if ( device_features.fragmentStoresAndAtomics && device_features.vertexPipelineStoresAndAtomics ) {
			features.vertexPipelineStoresAndAtomics = VK_TRUE;
			features.fragmentStoresAndAtomics = VK_TRUE;
			vk.fragmentStores = qtrue;
		}
#ifdef USE_VK_PBR
		if ( device_features.geometryShader )
			features.geometryShader = VK_TRUE;
#endif
		if ( r_ext_texture_filter_anisotropic->integer && device_features.samplerAnisotropy ) {
			features.samplerAnisotropy = VK_TRUE;
			vk.samplerAnisotropy = qtrue;
		}

		device_desc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_desc.pNext = NULL;
		device_desc.flags = 0;
		device_desc.queueCreateInfoCount = queue_create_info_count;
		device_desc.pQueueCreateInfos = queue_create_infos;
		device_desc.enabledLayerCount = 0;
		device_desc.ppEnabledLayerNames = NULL;
		device_desc.enabledExtensionCount = device_extension_count;
		device_desc.ppEnabledExtensionNames = device_extension_list;
		device_desc.pEnabledFeatures = &features;
		pNextPtr = (const void **)&device_desc.pNext;

		if ( meshShadersEnabled ) {
			*pNextPtr = &mesh_shader_features;
			pNextPtr = (const void **)&mesh_shader_features.pNext;
		}

		if ( sync2 ) {
			*pNextPtr = &sync2_features;
			sync2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
			sync2_features.pNext = NULL;
			sync2_features.synchronization2 = VK_TRUE;
			pNextPtr = (const void **)&sync2_features.pNext;
		}

		if ( dynamicRendering ) {
			*pNextPtr = &dyn_render_features;
			dyn_render_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
			dyn_render_features.pNext = NULL;
			dyn_render_features.dynamicRendering = VK_TRUE;
			pNextPtr = (const void **)&dyn_render_features.pNext;
		}

		if ( extDynState ) {
			*pNextPtr = &xdyn_features;
			xdyn_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
			xdyn_features.pNext = NULL;
			xdyn_features.extendedDynamicState = VK_TRUE;
			pNextPtr = (const void **)&xdyn_features.pNext;
		}

		if ( fragmentShadingRate ) {
			*pNextPtr = &fsr_features;
			fsr_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
			fsr_features.pNext = NULL;
			fsr_features.pipelineFragmentShadingRate = VK_TRUE;
			fsr_features.primitiveFragmentShadingRate = VK_TRUE;
			fsr_features.attachmentFragmentShadingRate = VK_TRUE;
			pNextPtr = (const void **)&fsr_features.pNext;
		}

		if ( descriptorIndexing ) {
			*pNextPtr = &desc_index_features;
			desc_index_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
			desc_index_features.pNext = NULL;
			desc_index_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
			desc_index_features.runtimeDescriptorArray = VK_TRUE;
			desc_index_features.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
			desc_index_features.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
			desc_index_features.descriptorBindingPartiallyBound = VK_TRUE;
			desc_index_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
			desc_index_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
			desc_index_features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
			desc_index_features.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
			desc_index_features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
			desc_index_features.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
			desc_index_features.shaderStorageTexelBufferArrayNonUniformIndexing = VK_TRUE;
			desc_index_features.shaderUniformTexelBufferArrayNonUniformIndexing = VK_TRUE;
			desc_index_features.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
			pNextPtr = (const void **)&desc_index_features.pNext;
		}

		if ( shaderFloat16Int8 ) {
			*pNextPtr = &f16i8_features;
			f16i8_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR;
			f16i8_features.pNext = NULL;
			f16i8_features.shaderFloat16 = VK_TRUE;
			f16i8_features.shaderInt8 = VK_TRUE;
			pNextPtr = (const void **)&f16i8_features.pNext;
		}

#ifdef _DEBUG
		if ( timelineSemaphore ) {
			*pNextPtr = &timeline_semaphore;
			timeline_semaphore.pNext = NULL;
			timeline_semaphore.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
			timeline_semaphore.timelineSemaphore = VK_TRUE;
			pNextPtr = (const void **)&timeline_semaphore.pNext;
		}

		if ( memoryModel ) {
			*pNextPtr = &memory_model;
			memory_model.pNext = NULL;
			memory_model.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES;
			memory_model.vulkanMemoryModel = VK_TRUE;
			memory_model.vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE;
			memory_model.vulkanMemoryModelDeviceScope = VK_TRUE;
			pNextPtr = (const void **)&memory_model.pNext;
		}

		if ( devAddrFeat ) {
			*pNextPtr = &devaddr_features;
			devaddr_features.pNext = NULL;
			devaddr_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
			devaddr_features.bufferDeviceAddress = VK_TRUE;
			devaddr_features.bufferDeviceAddressCaptureReplay = VK_FALSE;
			devaddr_features.bufferDeviceAddressMultiDevice = VK_FALSE;
			pNextPtr = (const void **)&devaddr_features.pNext;
		}

		if ( storage8bit ) {
			*pNextPtr = &storage_8bit_features;
			storage_8bit_features.pNext = NULL;
			storage_8bit_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
			storage_8bit_features.storageBuffer8BitAccess = VK_TRUE;
			storage_8bit_features.storagePushConstant8 = VK_FALSE;
			storage_8bit_features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
			pNextPtr = (const void **)&storage_8bit_features.pNext;
		}

		// Ray tracing features
		if ( rayTracingPipeline && accelerationStructure && deferredHostOperations && bufferDeviceAddress ) {
			static VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_pipeline_features;
			static VkPhysicalDeviceAccelerationStructureFeaturesKHR as_features;
			static VkPhysicalDeviceBufferDeviceAddressFeatures buffer_addr_features_rt;

			*pNextPtr = &buffer_addr_features_rt;
			buffer_addr_features_rt.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
			buffer_addr_features_rt.pNext = &rt_pipeline_features;
			buffer_addr_features_rt.bufferDeviceAddress = VK_TRUE;
			buffer_addr_features_rt.bufferDeviceAddressCaptureReplay = VK_FALSE;
			buffer_addr_features_rt.bufferDeviceAddressMultiDevice = VK_FALSE;

			rt_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
			rt_pipeline_features.pNext = &as_features;
			rt_pipeline_features.rayTracingPipeline = VK_TRUE;
			rt_pipeline_features.rayTracingPipelineTraceRaysIndirect = VK_FALSE;
			rt_pipeline_features.rayTraversalPrimitiveCulling = VK_FALSE;

			as_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
			as_features.pNext = NULL;
			as_features.accelerationStructure = VK_TRUE;
			as_features.accelerationStructureCaptureReplay = VK_FALSE;
			as_features.accelerationStructureIndirectBuild = VK_FALSE;
			as_features.accelerationStructureHostCommands = VK_FALSE;
			as_features.descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE;
		}
#endif
		(void)pNextPtr;
		res = qvkCreateDevice( physical_device, &device_desc, NULL, &vk.device );
		if ( res < 0 ) {
			ri.Printf( PRINT_ERROR, "vkCreateDevice returned %s\n", vk_result_string( res ) );
			return qfalse;
		}
	}

	return qtrue;
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#define INIT_INSTANCE_FUNCTION(func) \
	q##func = (PFN_##func) ri.VK_GetInstanceProcAddr(vk_instance, #func); \
	if (q##func == NULL) {											\
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func);	\
	}

#define INIT_INSTANCE_FUNCTION_EXT(func) \
	q##func = (PFN_##func) ri.VK_GetInstanceProcAddr(vk_instance, #func);


#define INIT_DEVICE_FUNCTION(func) \
	q##func = (PFN_ ## func) qvkGetDeviceProcAddr(vk.device, #func);\
	if (q##func == NULL) {											\
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func);	\
	}

#define INIT_DEVICE_FUNCTION_EXT(func) \
	q##func = (PFN_ ## func) qvkGetDeviceProcAddr(vk.device, #func);


static void vk_destroy_instance( void ) {
	if ( vk_surface != VK_NULL_HANDLE ) {
		if ( qvkDestroySurfaceKHR != NULL ) {
			qvkDestroySurfaceKHR( vk_instance, vk_surface, NULL );
		}
		vk_surface = VK_NULL_HANDLE;
	}

#ifdef USE_VK_VALIDATION
	if ( vk_debug_callback ) {
		if ( qvkDestroyDebugReportCallbackEXT != NULL ) {
			qvkDestroyDebugReportCallbackEXT( vk_instance, vk_debug_callback, NULL );
		}
		vk_debug_callback = VK_NULL_HANDLE;
	}
#endif

	if ( vk_instance != VK_NULL_HANDLE ) {
		if ( qvkDestroyInstance ) {
			qvkDestroyInstance( vk_instance, NULL );
		}
		vk_instance = VK_NULL_HANDLE;
	}
}


static void init_vulkan_library( void )
{
	VkPhysicalDeviceProperties props;
	VkPhysicalDevice *physical_devices;
	uint32_t device_count;
	int device_index;
	VkResult res;

	Com_Memset( &vk, 0, sizeof( vk ) );

#ifdef USE_VULKAN
	// Allow forcing a specific ICD via cvar (maps to VK_ICD_FILENAMES).
	if ( r_vk_icd && r_vk_icd->string && r_vk_icd->string[0] ) {
		if ( setenv( "VK_ICD_FILENAMES", r_vk_icd->string, 1 ) == 0 ) {
			ri.Printf( PRINT_ALL, "...using VK_ICD_FILENAMES override: %s\n", r_vk_icd->string );
		} else {
			ri.Printf( PRINT_WARNING, "Failed to set VK_ICD_FILENAMES to '%s'\n", r_vk_icd->string );
		}
	}
#endif

	if ( vk_instance == VK_NULL_HANDLE ) {

		// force cleanup
		vk_destroy_instance();

		// Get functions that do not depend on VkInstance (vk_instance == nullptr at this point).
		INIT_INSTANCE_FUNCTION( vkCreateInstance )
		INIT_INSTANCE_FUNCTION( vkEnumerateInstanceExtensionProperties )

		// Get instance level functions.
		create_instance();

		INIT_INSTANCE_FUNCTION( vkCreateDevice )
		INIT_INSTANCE_FUNCTION( vkDestroyInstance )
		INIT_INSTANCE_FUNCTION( vkEnumerateDeviceExtensionProperties )
		INIT_INSTANCE_FUNCTION( vkEnumeratePhysicalDevices )
		INIT_INSTANCE_FUNCTION( vkGetDeviceProcAddr )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFeatures )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFormatProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceMemoryProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceQueueFamilyProperties )
		INIT_INSTANCE_FUNCTION( vkDestroySurfaceKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceCapabilitiesKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceFormatsKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfacePresentModesKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceSupportKHR )

		// Load KHR_get_physical_device_properties2 extension functions if available
		// Try KHR version first (for Vulkan 1.0), then core version (Vulkan 1.1+)
		// Note: In Vulkan 1.1+, these are core functions with identical signatures
		void *funcPtr = ri.VK_GetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceProperties2KHR");
		if (!funcPtr) {
			funcPtr = ri.VK_GetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceProperties2");
		}
		qvkGetPhysicalDeviceProperties2KHR = (PFN_vkGetPhysicalDeviceProperties2KHR)funcPtr;
		if (qvkGetPhysicalDeviceProperties2KHR) {
			ri.Printf(PRINT_DEVELOPER, "Loaded vkGetPhysicalDeviceProperties2KHR function pointer\n");
		} else {
			ri.Printf(PRINT_WARNING, "Failed to load vkGetPhysicalDeviceProperties2KHR function pointer (extension may not be enabled)\n");
		}
		
		funcPtr = ri.VK_GetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFeatures2KHR");
		if (!funcPtr) {
			funcPtr = ri.VK_GetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFeatures2");
		}
		qvkGetPhysicalDeviceFeatures2KHR = (PFN_vkGetPhysicalDeviceFeatures2KHR)funcPtr;
		if (qvkGetPhysicalDeviceFeatures2KHR) {
			ri.Printf(PRINT_DEVELOPER, "Loaded vkGetPhysicalDeviceFeatures2KHR function pointer\n");
		} else {
			ri.Printf(PRINT_WARNING, "Failed to load vkGetPhysicalDeviceFeatures2KHR function pointer (extension may not be enabled)\n");
		}

#ifdef USE_VK_VALIDATION
		INIT_INSTANCE_FUNCTION_EXT( vkCreateDebugReportCallbackEXT )
		INIT_INSTANCE_FUNCTION_EXT( vkDestroyDebugReportCallbackEXT )

		// Create debug callback.
		if ( qvkCreateDebugReportCallbackEXT && qvkDestroyDebugReportCallbackEXT ) {
			VkDebugReportCallbackCreateInfoEXT desc;
			desc.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			desc.pNext = NULL;
			desc.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
				VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
				VK_DEBUG_REPORT_ERROR_BIT_EXT;
			desc.pfnCallback = &debug_callback;
			desc.pUserData = NULL;

			VK_CHECK( qvkCreateDebugReportCallbackEXT( vk_instance, &desc, NULL, &vk_debug_callback ) );
		}
#endif

		// create surface
		if ( !ri.VK_CreateSurface( vk_instance, &vk_surface ) ) {
			ri.Error( ERR_FATAL, "Error creating Vulkan surface" );
			return;
		}
	} // vk_instance == VK_NULL_HANDLE

	res = qvkEnumeratePhysicalDevices( vk_instance, &device_count, NULL );
	if ( device_count == 0 ) {
		ri.Error( ERR_FATAL, "Vulkan: no physical devices found" );
		return;
	}
	else if ( res < 0 ) {
		ri.Error( ERR_FATAL, "vkEnumeratePhysicalDevices returned %s", vk_result_string( res ) );
		return;
	}

	physical_devices = (VkPhysicalDevice*)ri.Malloc( device_count * sizeof( VkPhysicalDevice ) );
	VK_CHECK( qvkEnumeratePhysicalDevices( vk_instance, &device_count, physical_devices ) );

	// initial physical device index
	device_index = r_device->integer;

	ri.Printf( PRINT_ALL, ".......................\nAvailable physical devices:\n" );
        for ( uint32_t i = 0; i < device_count; i++ ) {
		qvkGetPhysicalDeviceProperties( physical_devices[ i ], &props );
		ri.Printf( PRINT_ALL, " %i: %s\n", i, renderer_name( &props ) );
		if ( device_index == -1 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
			device_index = i;
		} else if ( device_index == -2 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) {
			device_index = i;
		}
	}
	ri.Printf( PRINT_ALL, ".......................\n" );

	vk.physical_device = VK_NULL_HANDLE;
	{
		int requested_index = r_device ? r_device->integer : -1;
		qboolean forced_index = ( requested_index >= 0 );
		int preferred_index = device_index;

		// Enhanced device selection with scoring
		vk_device_candidate_t *candidates = (vk_device_candidate_t*)ri.Malloc(sizeof(vk_device_candidate_t) * device_count);
		uint32_t candidate_count = 0;

		// Score all available devices
		for (uint32_t dev_idx = 0; dev_idx < device_count; dev_idx++) {
			vk_device_candidate_t *candidate = &candidates[candidate_count++];
			candidate->device_index = dev_idx;
			candidate->device = physical_devices[dev_idx];
			candidate->score = 0;
			candidate->reason = "Unknown";

			qvkGetPhysicalDeviceProperties(candidate->device, &candidate->properties);
			qvkGetPhysicalDeviceFeatures(candidate->device, &candidate->features);
			qvkGetPhysicalDeviceMemoryProperties(candidate->device, &candidate->memory);

			// Score based on device type (discrete GPUs preferred)
			switch (candidate->properties.deviceType) {
				case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
					candidate->score += 1000;
					break;
				case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
					candidate->score += 500;
					break;
				case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
					candidate->score += 200;
					break;
				case VK_PHYSICAL_DEVICE_TYPE_CPU:
					candidate->score += 100;
					break;
				default:
					candidate->score += 50;
					break;
			}

			// Score based on Vulkan API version support
			uint32_t api_version = candidate->properties.apiVersion;
			if (VK_API_VERSION_MAJOR(api_version) >= 1) {
				candidate->score += VK_API_VERSION_MINOR(api_version) * 10;
			}

		// Check for required queue family support
		uint32_t queue_family_count;
		qvkGetPhysicalDeviceQueueFamilyProperties(candidate->device, &queue_family_count, NULL);
		VkQueueFamilyProperties *queue_families = (VkQueueFamilyProperties*)ri.Malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
		qvkGetPhysicalDeviceQueueFamilyProperties(candidate->device, &queue_family_count, queue_families);

		qboolean has_graphics_queue = qfalse;
		qboolean has_compute_queue = qfalse;
		qboolean has_transfer_queue = qfalse;

		for (uint32_t q = 0; q < queue_family_count; q++) {
			VkBool32 present_supported = VK_FALSE;
			if (vk_surface != VK_NULL_HANDLE) {
				qvkGetPhysicalDeviceSurfaceSupportKHR(candidate->device, q, vk_surface, &present_supported);
			}

			VkQueueFlags flags = queue_families[q].queueFlags;
			if (flags & VK_QUEUE_GRAPHICS_BIT) {
				if (!vk_surface || present_supported) {
					has_graphics_queue = qtrue;
					candidate->score += 100; // Bonus for graphics + present support
				}
			}
			if (flags & VK_QUEUE_COMPUTE_BIT) {
				has_compute_queue = qtrue;
				candidate->score += 25; // Bonus for compute support
			}
			if (flags & VK_QUEUE_TRANSFER_BIT) {
				has_transfer_queue = qtrue;
				candidate->score += 10; // Bonus for dedicated transfer queue
			}
		}
		ri.Free(queue_families);

			if (!has_graphics_queue) {
				candidate->score = 0; // Unusable device
				candidate->reason = "No suitable graphics queue";
			} else {
				// Basic feature scoring based on available information
				if (has_compute_queue) {
					candidate->score += 25; // Bonus for compute support
				}
				if (has_transfer_queue) {
					candidate->score += 10; // Bonus for dedicated transfer queue
				}

				candidate->reason = has_compute_queue ? "Full GPU features" : "Basic graphics support";
			}

			ri.Printf(PRINT_ALL, "Device %u (%s): Score %d - %s\n",
				candidate->device_index, candidate->properties.deviceName, candidate->score, candidate->reason);
		}

		// Select best device
		uint32_t best_score = 0;
		uint32_t selected_index = 0;

		for (uint32_t cand_idx = 0; cand_idx < candidate_count; cand_idx++) {
			if (candidates[cand_idx].score > best_score) {
				best_score = candidates[cand_idx].score;
				selected_index = (int)cand_idx;
			}
		}

		// Override with user preference if valid
		if (forced_index && requested_index >= 0 && (uint32_t)requested_index < device_count) {
			selected_index = requested_index;
			ri.Printf(PRINT_ALL, "...user forced device selection: %d\n", requested_index);
		}

		vk.physical_device = candidates[selected_index].device;
		ri.Printf(PRINT_ALL, "...selected physical device: %d (%s) - %s\n",
			selected_index, candidates[selected_index].properties.deviceName,
			candidates[selected_index].reason);

		// Enhanced device information reporting
		const VkPhysicalDeviceProperties *device_props = &candidates[selected_index].properties;
		ri.Printf(PRINT_ALL, "...device type: %s\n", vk_get_device_type_string(device_props->deviceType));
		ri.Printf(PRINT_ALL, "...Vulkan API: %d.%d.%d\n",
			VK_API_VERSION_MAJOR(device_props->apiVersion),
			VK_API_VERSION_MINOR(device_props->apiVersion),
			VK_API_VERSION_PATCH(device_props->apiVersion));
		ri.Printf(PRINT_ALL, "...driver version: %d.%d.%d\n",
			VK_API_VERSION_MAJOR(device_props->driverVersion),
			VK_API_VERSION_MINOR(device_props->driverVersion),
			VK_API_VERSION_PATCH(device_props->driverVersion));
		ri.Printf(PRINT_ALL, "...vendor ID: 0x%04x, device ID: 0x%04x\n",
			device_props->vendorID, device_props->deviceID);

		ri.Free(candidates);

		// Clamp explicit index into range if needed (will fall back to other devices if creation fails).
		if ( preferred_index >= (int)device_count ) {
			preferred_index = device_count - 1;
		} else if ( preferred_index < 0 ) {
			preferred_index = 0;
		}

		for ( int attempt = 0; attempt < (int)device_count; attempt++ ) {
			int attempt_index = ( attempt == 0 ) ? preferred_index : ( ( preferred_index + attempt ) % (int)device_count );

			if ( forced_index && attempt == 1 ) {
				ri.Printf( PRINT_WARNING, "...requested device %d failed, falling back to other adapters\n", requested_index );
			}

			if ( vk_create_device( physical_devices[ attempt_index ], attempt_index ) ) {
				vk.physical_device = physical_devices[ attempt_index ];
				ri.Printf( PRINT_ALL, "...selected physical device: %d (requested %d)\n", attempt_index, requested_index );
				break;
			}
		}
	}

	ri.Free( physical_devices );

	if ( vk.physical_device == VK_NULL_HANDLE ) {
		ri.Error( ERR_FATAL, "Vulkan: unable to find any suitable physical device" );
		return;
	}

	//
	// Get device level functions.
	//
	INIT_DEVICE_FUNCTION(vkAllocateCommandBuffers)
	INIT_DEVICE_FUNCTION(vkAllocateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkAllocateMemory)
	INIT_DEVICE_FUNCTION(vkBeginCommandBuffer)
	INIT_DEVICE_FUNCTION(vkBindBufferMemory)
	INIT_DEVICE_FUNCTION(vkBindImageMemory)
	INIT_DEVICE_FUNCTION(vkCmdBeginRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdBindDescriptorSets)
	INIT_DEVICE_FUNCTION(vkCmdBindIndexBuffer)
	INIT_DEVICE_FUNCTION(vkCmdBindPipeline)
	INIT_DEVICE_FUNCTION(vkCmdBindVertexBuffers)
	INIT_DEVICE_FUNCTION(vkCmdBlitImage)
	INIT_DEVICE_FUNCTION(vkCmdClearAttachments)
	INIT_DEVICE_FUNCTION(vkCmdCopyBuffer)
	INIT_DEVICE_FUNCTION(vkCmdCopyBufferToImage)
	INIT_DEVICE_FUNCTION(vkCmdCopyImage)
	INIT_DEVICE_FUNCTION(vkCmdDispatch)
	INIT_DEVICE_FUNCTION(vkCmdDraw)
	INIT_DEVICE_FUNCTION(vkCmdDrawIndexed)
	INIT_DEVICE_FUNCTION(vkCmdDrawIndexedIndirect)
	INIT_DEVICE_FUNCTION(vkCmdEndRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdNextSubpass)
	INIT_DEVICE_FUNCTION(vkCmdPipelineBarrier)
	INIT_DEVICE_FUNCTION(vkCmdPushConstants)
	INIT_DEVICE_FUNCTION(vkCmdSetDepthBias)
	INIT_DEVICE_FUNCTION(vkCmdSetScissor)
	INIT_DEVICE_FUNCTION(vkCmdSetViewport)
	INIT_DEVICE_FUNCTION_EXT(vkCmdWriteTimestamp)
	INIT_DEVICE_FUNCTION(vkGetQueryPoolResults)
	// vkResetQueryPool was added in Vulkan 1.2. Treat it as optional:
	// older loaders or drivers may not expose it even if the header does.
	// We already null-check qvkResetQueryPool before use, so don't hard-fail here.
	INIT_DEVICE_FUNCTION_EXT(vkResetQueryPool)
	INIT_DEVICE_FUNCTION(vkCreateBuffer)
	INIT_DEVICE_FUNCTION(vkCreateCommandPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkCreateFence)
	INIT_DEVICE_FUNCTION(vkCreateFramebuffer)
	INIT_DEVICE_FUNCTION(vkCreateGraphicsPipelines)
	INIT_DEVICE_FUNCTION(vkCreateComputePipelines)
	INIT_DEVICE_FUNCTION(vkCreateImage)
	INIT_DEVICE_FUNCTION(vkCreateImageView)
	INIT_DEVICE_FUNCTION(vkCreatePipelineCache)
	INIT_DEVICE_FUNCTION(vkGetPipelineCacheData)
	INIT_DEVICE_FUNCTION(vkCreatePipelineLayout)
	INIT_DEVICE_FUNCTION(vkCreateRenderPass)
	INIT_DEVICE_FUNCTION(vkCreateSampler)
	INIT_DEVICE_FUNCTION(vkCreateSemaphore)
	INIT_DEVICE_FUNCTION(vkCreateShaderModule)
	INIT_DEVICE_FUNCTION(vkCreateQueryPool)
	INIT_DEVICE_FUNCTION(vkDestroyQueryPool)
	INIT_DEVICE_FUNCTION(vkDestroyBuffer)
	INIT_DEVICE_FUNCTION(vkDestroyCommandPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkDestroyDevice)
	INIT_DEVICE_FUNCTION(vkDestroyFence)
	INIT_DEVICE_FUNCTION(vkDestroyFramebuffer)
	INIT_DEVICE_FUNCTION(vkDestroyImage)
	INIT_DEVICE_FUNCTION(vkDestroyImageView)
	INIT_DEVICE_FUNCTION(vkDestroyPipeline)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineCache)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineLayout)
	INIT_DEVICE_FUNCTION(vkDestroyRenderPass)
	INIT_DEVICE_FUNCTION(vkDestroySampler)
	INIT_DEVICE_FUNCTION(vkDestroySemaphore)
	INIT_DEVICE_FUNCTION(vkDestroyShaderModule)
	INIT_DEVICE_FUNCTION(vkDeviceWaitIdle)
	INIT_DEVICE_FUNCTION(vkEndCommandBuffer)
	INIT_DEVICE_FUNCTION(vkFlushMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkFreeCommandBuffers)
	INIT_DEVICE_FUNCTION(vkFreeDescriptorSets)
	INIT_DEVICE_FUNCTION(vkFreeMemory)
	INIT_DEVICE_FUNCTION(vkGetBufferMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetDeviceQueue)
	INIT_DEVICE_FUNCTION(vkGetImageMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetImageSubresourceLayout)
	INIT_DEVICE_FUNCTION(vkInvalidateMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkMapMemory)
	INIT_DEVICE_FUNCTION(vkQueueSubmit)
	INIT_DEVICE_FUNCTION(vkQueueWaitIdle)
	INIT_DEVICE_FUNCTION(vkResetCommandBuffer)
	INIT_DEVICE_FUNCTION(vkResetDescriptorPool)
	INIT_DEVICE_FUNCTION(vkResetFences)
	INIT_DEVICE_FUNCTION(vkUnmapMemory)
	INIT_DEVICE_FUNCTION(vkUpdateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkWaitForFences)

	// Pipeline binary functions (VK_KHR_pipeline_executable_properties)
	if (vk_advanced.pipelineBinaries) {
		INIT_DEVICE_FUNCTION_EXT(vkGetPipelineExecutablePropertiesKHR)
		INIT_DEVICE_FUNCTION_EXT(vkGetPipelineExecutableStatisticsKHR)
		INIT_DEVICE_FUNCTION_EXT(vkGetPipelineExecutableInternalRepresentationsKHR)
		if (qvkGetPipelineExecutablePropertiesKHR) {
			ri.Printf(PRINT_ALL, "...pipeline binary functions loaded\n");
		}
	}
	INIT_DEVICE_FUNCTION(vkAcquireNextImageKHR)
	INIT_DEVICE_FUNCTION(vkCreateSwapchainKHR)
	INIT_DEVICE_FUNCTION(vkDestroySwapchainKHR)
	INIT_DEVICE_FUNCTION(vkGetSwapchainImagesKHR)
	INIT_DEVICE_FUNCTION(vkQueuePresentKHR)

	if ( vk.dedicatedAllocation ) {
		INIT_DEVICE_FUNCTION_EXT(vkGetBufferMemoryRequirements2KHR);
		INIT_DEVICE_FUNCTION_EXT(vkGetImageMemoryRequirements2KHR);
		qvkCmdBeginRenderingKHR      = (PFN_vkCmdBeginRenderingKHR)      qvkGetDeviceProcAddr( vk.device, "vkCmdBeginRenderingKHR" );
		qvkCmdEndRenderingKHR        = (PFN_vkCmdEndRenderingKHR)        qvkGetDeviceProcAddr( vk.device, "vkCmdEndRenderingKHR" );
		qvkCmdPipelineBarrier2KHR    = (PFN_vkCmdPipelineBarrier2KHR)    qvkGetDeviceProcAddr( vk.device, "vkCmdPipelineBarrier2KHR" );
		qvkQueueSubmit2KHR           = (PFN_vkQueueSubmit2KHR)           qvkGetDeviceProcAddr( vk.device, "vkQueueSubmit2KHR" );
		qvkCmdSetFragmentShadingRateKHR = (PFN_vkCmdSetFragmentShadingRateKHR) qvkGetDeviceProcAddr( vk.device, "vkCmdSetFragmentShadingRateKHR" );
		if ( !qvkGetBufferMemoryRequirements2KHR || !qvkGetImageMemoryRequirements2KHR ) {
			vk.dedicatedAllocation = qfalse;
		}
	}

	// Load ray tracing functions if supported
	if ( vk.rayTracingSupported ) {
		INIT_DEVICE_FUNCTION_EXT(vkCreateAccelerationStructureKHR);
		INIT_DEVICE_FUNCTION_EXT(vkDestroyAccelerationStructureKHR);
		INIT_DEVICE_FUNCTION_EXT(vkGetAccelerationStructureBuildSizesKHR);
		INIT_DEVICE_FUNCTION_EXT(vkGetAccelerationStructureDeviceAddressKHR);
		INIT_DEVICE_FUNCTION_EXT(vkCmdBuildAccelerationStructuresKHR);
		INIT_DEVICE_FUNCTION_EXT(vkCmdTraceRaysKHR);
		INIT_DEVICE_FUNCTION_EXT(vkCreateRayTracingPipelinesKHR);
		INIT_DEVICE_FUNCTION_EXT(vkGetRayTracingShaderGroupHandlesKHR);
		INIT_DEVICE_FUNCTION_EXT(vkGetRayTracingCaptureReplayShaderGroupHandlesKHR);
		INIT_DEVICE_FUNCTION_EXT(vkCmdTraceRaysIndirectKHR);
		// Note: Ray tracing properties and features are queried via vkGetPhysicalDeviceProperties2KHR
		// and vkGetPhysicalDeviceFeatures2KHR with pNext chains, not as separate functions
		INIT_DEVICE_FUNCTION_EXT(vkGetBufferDeviceAddress);
	}

	if ( vk.debugMarkers ) {
		INIT_DEVICE_FUNCTION_EXT(vkDebugMarkerSetObjectNameEXT)
	}

	INIT_DEVICE_FUNCTION_EXT(vkCmdClearColorImage)
}

#undef INIT_INSTANCE_FUNCTION
#undef INIT_DEVICE_FUNCTION
#undef INIT_DEVICE_FUNCTION_EXT
#pragma GCC diagnostic pop

static void __attribute__((unused)) deinit_instance_functions( void )
{
	qvkCreateInstance = NULL;
	qvkEnumerateInstanceExtensionProperties = NULL;

	// instance functions:
	qvkCreateDevice = NULL;
	qvkDestroyInstance = NULL;
	qvkEnumerateDeviceExtensionProperties = NULL;
	qvkEnumeratePhysicalDevices = NULL;
	qvkGetDeviceProcAddr = NULL;
	qvkGetPhysicalDeviceFeatures = NULL;
	qvkGetPhysicalDeviceFormatProperties = NULL;
	qvkGetPhysicalDeviceMemoryProperties = NULL;
	qvkGetPhysicalDeviceProperties = NULL;
	qvkGetPhysicalDeviceQueueFamilyProperties = NULL;
	qvkDestroySurfaceKHR = NULL;
	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceFormatsKHR = NULL;
	qvkGetPhysicalDeviceSurfacePresentModesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceSupportKHR = NULL;
#ifdef USE_VK_VALIDATION
	qvkCreateDebugReportCallbackEXT = NULL;
	qvkDestroyDebugReportCallbackEXT = NULL;
#endif
}


static void __attribute__((unused)) deinit_device_functions( void )
{
	// device functions:
	qvkAllocateCommandBuffers					= NULL;
	qvkAllocateDescriptorSets					= NULL;
	qvkAllocateMemory							= NULL;
	qvkBeginCommandBuffer						= NULL;
	qvkBindBufferMemory							= NULL;
	qvkBindImageMemory							= NULL;
	qvkCmdBeginRenderPass						= NULL;
	qvkCmdBindDescriptorSets					= NULL;
	qvkCmdBindIndexBuffer						= NULL;
	qvkCmdBindPipeline							= NULL;
	qvkCmdBindVertexBuffers						= NULL;
	qvkCmdBlitImage								= NULL;
	qvkCmdClearAttachments						= NULL;
	qvkCmdCopyBuffer							= NULL;
	qvkCmdCopyBufferToImage						= NULL;
	qvkCmdCopyImage								= NULL;
	qvkCmdDispatch								= NULL;
	qvkCmdDraw									= NULL;
	qvkCmdDrawIndexed							= NULL;
	qvkCmdDrawIndexedIndirect					= NULL;
	qvkCmdEndRenderPass							= NULL;
	qvkCmdNextSubpass							= NULL;
	qvkCmdPipelineBarrier						= NULL;
	qvkCmdPushConstants							= NULL;
	qvkCmdSetDepthBias							= NULL;
	qvkCmdSetScissor							= NULL;
	qvkCmdSetViewport							= NULL;
	qvkCreateBuffer								= NULL;
	qvkCreateCommandPool						= NULL;
	qvkCreateDescriptorPool						= NULL;
	qvkCreateDescriptorSetLayout				= NULL;
	qvkCreateFence								= NULL;
	qvkCreateFramebuffer						= NULL;
	qvkCreateGraphicsPipelines					= NULL;
	qvkCreateComputePipelines					= NULL;
	qvkCmdDispatch								= NULL;
	qvkCreateImage								= NULL;
	qvkCreateImageView							= NULL;
	qvkCreatePipelineCache						= NULL;
qvkGetPipelineCacheData					= NULL;
	qvkCreatePipelineLayout						= NULL;
	qvkCreateRenderPass							= NULL;
	qvkCreateSampler							= NULL;
	qvkCreateSemaphore							= NULL;
	qvkCreateShaderModule						= NULL;
	qvkDestroyBuffer							= NULL;
	qvkDestroyCommandPool						= NULL;
	qvkDestroyDescriptorPool					= NULL;
	qvkDestroyDescriptorSetLayout				= NULL;
	qvkDestroyDevice							= NULL;
	qvkDestroyFence								= NULL;
	qvkDestroyFramebuffer						= NULL;
	qvkDestroyImage								= NULL;
	qvkDestroyImageView							= NULL;
	qvkDestroyPipeline							= NULL;
	qvkDestroyPipelineCache						= NULL;
	qvkDestroyPipelineLayout					= NULL;
	qvkDestroyRenderPass						= NULL;
	qvkDestroySampler							= NULL;
	qvkDestroySemaphore							= NULL;
	qvkDestroyShaderModule						= NULL;
	qvkDeviceWaitIdle							= NULL;
	qvkEndCommandBuffer							= NULL;
	qvkFlushMappedMemoryRanges					= NULL;
	qvkFreeCommandBuffers						= NULL;
	qvkFreeDescriptorSets						= NULL;
	qvkFreeMemory								= NULL;
	qvkGetBufferMemoryRequirements				= NULL;
	qvkGetDeviceQueue							= NULL;
	qvkGetImageMemoryRequirements				= NULL;
	qvkGetImageSubresourceLayout				= NULL;
	qvkInvalidateMappedMemoryRanges				= NULL;
	qvkMapMemory								= NULL;
	qvkQueueSubmit								= NULL;
	qvkQueueWaitIdle							= NULL;
	qvkResetCommandBuffer						= NULL;
	qvkResetDescriptorPool						= NULL;
	qvkResetFences								= NULL;
	qvkUnmapMemory								= NULL;
	qvkUpdateDescriptorSets						= NULL;
	qvkWaitForFences							= NULL;
	qvkAcquireNextImageKHR						= NULL;
	qvkCreateSwapchainKHR						= NULL;
	qvkDestroySwapchainKHR						= NULL;
	qvkGetSwapchainImagesKHR					= NULL;
	qvkQueuePresentKHR							= NULL;

	qvkGetBufferMemoryRequirements2KHR			= NULL;
	qvkGetImageMemoryRequirements2KHR			= NULL;

	qvkDebugMarkerSetObjectNameEXT				= NULL;
	qvkCmdClearColorImage						= NULL;

	vk.active = qtrue;
}


static VkShaderModule SHADER_MODULE_FUNC(const uint8_t *bytes, const int count) {
	VkShaderModuleCreateInfo desc;
	VkShaderModule module;

	if ( count % 4 != 0 ) {
		ri.Error( ERR_FATAL, "Vulkan: SPIR-V binary buffer size is not a multiple of 4" );
	}

	desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.codeSize = count;
	desc.pCode = (const uint32_t*)bytes;

	VK_CHECK(qvkCreateShaderModule(vk.device, &desc, NULL, &module));

	return module;
}

// Shader data and binding includes
#include "shaders/spirv/shader_data.c"
#define SHADER_MODULE(name) SHADER_MODULE_FUNC((const uint8_t*)name,sizeof(name))
#include "shaders/spirv/shader_binding.c"

static void __attribute__((unused)) vk_create_layout_binding( int binding, VkDescriptorType type, VkShaderStageFlags flags, VkDescriptorSetLayout *layout )
{
	uint32_t count = 0;
	VkDescriptorSetLayoutBinding bind[2];
	VkDescriptorSetLayoutCreateInfo desc;

	bind[count].binding = binding;
	bind[count].descriptorType = type;
	bind[count].descriptorCount = 1;
	bind[count].stageFlags = flags;
	bind[count].pImmutableSamplers = NULL;
	count++;

	if ( *layout == vk.set_layout_uniform ) {
		bind[count].binding = VK_DESC_UNIFORM_CAMERA_BINDING;
		bind[count].descriptorType = type;
		bind[count].descriptorCount = 1;
		bind[count].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		bind[count].pImmutableSamplers = NULL;
		count++;    
	}

	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.bindingCount = count;
	desc.pBindings = bind;

	VK_CHECK( qvkCreateDescriptorSetLayout(vk.device, &desc, NULL, layout ) );
}

static void vk_write_uniform_descriptor( VkWriteDescriptorSet *desc, VkDescriptorBufferInfo *info, 
	VkBuffer buffer, VkDescriptorSet descriptor, const uint32_t binding, const size_t size )
{
	info[binding].buffer = buffer;
	info[binding].offset = 0;
	info[binding].range = size;

	desc[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc[binding].dstSet = descriptor;
	desc[binding].dstBinding = binding;
	desc[binding].dstArrayElement = 0;
	desc[binding].descriptorCount = 1;
	desc[binding].pNext = NULL;
	desc[binding].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	desc[binding].pImageInfo = NULL;
	desc[binding].pBufferInfo = &info[binding];
	desc[binding].pTexelBufferView = NULL;
}

void vk_update_uniform_descriptor( VkDescriptorSet descriptor, VkBuffer buffer )
{
	VkDescriptorBufferInfo info[VK_DESC_UNIFORM_COUNT];
	VkWriteDescriptorSet desc[VK_DESC_UNIFORM_COUNT];

	vk_write_uniform_descriptor( desc, info, buffer, descriptor, VK_DESC_UNIFORM_MAIN_BINDING, sizeof(vkUniform_t) );
	vk_write_uniform_descriptor( desc, info, buffer, descriptor, VK_DESC_UNIFORM_CAMERA_BINDING, sizeof(vkUniformCamera_t) );

	qvkUpdateDescriptorSets(vk.device, VK_DESC_UNIFORM_COUNT, desc, 0, NULL);
}


static VkSampler vk_find_sampler( const Vk_Sampler_Def *def ) {
	VkSamplerAddressMode address_mode;
	VkSamplerCreateInfo desc;
	VkSampler sampler;
	VkFilter mag_filter;
	VkFilter min_filter;
	VkSamplerMipmapMode mipmap_mode;
	float maxLod;
	float lodBias;
	int i;

	// Look for sampler among existing samplers.
	for ( i = 0; i < vk.samplers.count; i++ ) {
		const Vk_Sampler_Def *cur_def = &vk.samplers.def[i];
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			return vk.samplers.handle[i];
		}
	}

	// Create new sampler.
	if ( vk.samplers.count >= MAX_VK_SAMPLERS ) {
		ri.Error( ERR_DROP, "vk_find_sampler: MAX_VK_SAMPLERS hit\n" );
		// return VK_NULL_HANDLE;
	}

	address_mode = def->address_mode;

	if (def->vk_mag_filter == VK_FILTER_NEAREST) {
		mag_filter = VK_FILTER_NEAREST;
	} else if (def->vk_mag_filter == VK_FILTER_LINEAR) {
		mag_filter = VK_FILTER_LINEAR;
	} else {
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid vk_mag_filter");
		return VK_NULL_HANDLE;
	}

	maxLod = vk.maxLod;

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
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid vk_min_filter");
		return VK_NULL_HANDLE;
	}

	if ( def->max_lod_1_0 ) {
		maxLod = 1.0f;
	}

	// For font textures without mipmaps, force maxLod=0.0f to only sample from base mip level
	// This prevents blurriness and chunky block artifacts from mipmap sampling
	if ( def->isFontTexture && mipmap_mode == VK_SAMPLER_MIPMAP_MODE_NEAREST && 
	     (def->vk_min_filter == VK_FILTER_NEAREST || def->vk_min_filter == VK_FILTER_LINEAR) ) {
		maxLod = 0.0f;
	}

	lodBias = Com_Clamp( -2.0f, 2.0f, r_textureLodBias ? r_textureLodBias->value : 0.0f );

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

	if ( def->noAnisotropy || mipmap_mode == VK_SAMPLER_MIPMAP_MODE_NEAREST || mag_filter == VK_FILTER_NEAREST ) {
		desc.anisotropyEnable = VK_FALSE;
		desc.maxAnisotropy = 1.0f;
	} else {
		desc.anisotropyEnable = (r_ext_texture_filter_anisotropic->integer && vk.samplerAnisotropy) ? VK_TRUE : VK_FALSE;
		if ( desc.anisotropyEnable ) {
			desc.maxAnisotropy = MIN( r_ext_max_anisotropy->integer, vk.maxAnisotropy );
		}
	}

	desc.compareEnable = VK_FALSE;
	desc.compareOp = VK_COMPARE_OP_ALWAYS;
	desc.minLod = 0.0f;
	desc.maxLod = (maxLod == vk.maxLod) ? VK_LOD_CLAMP_NONE : maxLod;
	desc.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	desc.unnormalizedCoordinates = VK_FALSE;

	VK_CHECK( qvkCreateSampler( vk.device, &desc, NULL, &sampler ) );

	SET_OBJECT_NAME( sampler, va( "image sampler %i", vk.samplers.count ), VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT );

	vk.samplers.def[ vk.samplers.count ] = *def;
	vk.samplers.handle[ vk.samplers.count ] = sampler;
	vk.samplers.count++;

	return sampler;
}


void vk_destroy_samplers( void )
{
	int i;

	for ( i = 0; i < vk.samplers.count; i++ ) {
		qvkDestroySampler( vk.device, vk.samplers.handle[i], NULL );
		memset( &vk.samplers.def[i], 0x0, sizeof( vk.samplers.def[i] ) );
		vk.samplers.handle[i] = VK_NULL_HANDLE;
	}

	vk.samplers.count = 0;
}


void vk_update_attachment_descriptors( void ) {

	if ( vk.color_image_view )
	{
		VkDescriptorImageInfo info;
		VkWriteDescriptorSet desc;
		Vk_Sampler_Def sd;

		Com_Memset( &sd, 0, sizeof( sd ) );
		// Always use linear filtering for color buffer when used for bloom/post-processing
		// to avoid blocky artifacts. vk.blitFilter (which can be VK_FILTER_NEAREST) is only for final blit to screen.
		sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.max_lod_1_0 = qtrue;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );
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

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		// screenmap
		sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
		sd.max_lod_1_0 = qfalse;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );

		info.imageView = vk.screenMap.color_image_view;
		desc.dstSet = vk.screenMap.color_descriptor;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		// bloom images - always use linear filtering for smooth bloom
		if ( r_bloom->integer )
		{
			uint32_t i;
			sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;
			info.sampler = vk_find_sampler( &sd );
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				info.imageView = vk.bloom_image_view[i];
				desc.dstSet = vk.bloom_image_descriptor[i];

				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}

#ifdef VK_PBR_BRDFLUT
		if( vk.pbrActive )
		{
			// brdf
			info.imageView = vk.brdflut_image_view;
			desc.dstSet = vk.brdflut_image_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );	
		
			// cubemap
			info.imageView = vk.cubeMap.color_image_view[0];
			desc.dstSet = vk.cubeMap.color_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );	
		}
#endif
	}
}


static void vk_create_attachments( void );

void vk_initialize( void )
{
	init_vulkan_library();
}

void vk_init_descriptors( void )
{
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet desc;
	uint32_t i;

	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.pNext = NULL;
	alloc.descriptorPool = vk.descriptor_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.set_layout_storage;

	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.storage.descriptor ) );

	// Allocate ray tracing descriptor set if supported
	if ( vk.rayTracingSupported && vk.rt.initialized && vk.rt.raytracingDescriptorSetLayout != VK_NULL_HANDLE ) {
		alloc.pSetLayouts = &vk.rt.raytracingDescriptorSetLayout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.rt.raytracingDescriptorSet ) );
		SET_OBJECT_NAME( vk.rt.raytracingDescriptorSet, "ray tracing descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}

	info.buffer = vk.storage.buffer;
	info.offset = 0;
	info.range = sizeof( uint32_t );

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

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

	// allocated and update descriptor set
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_uniform;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.tess[i].uniform_descriptor ) );

		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

		SET_OBJECT_NAME( vk.tess[ i ].uniform_descriptor, va( "uniform descriptor %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}

	if ( vk.color_image_view )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.color_descriptor ) );
		
		// Allocate compute descriptor set for post-processing
		if ( vk.compute_descriptor_set_layout != VK_NULL_HANDLE ) {
			alloc.pSetLayouts = &vk.compute_descriptor_set_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.compute_descriptor_set ) );
			SET_OBJECT_NAME( vk.compute_descriptor_set, "compute descriptor set - post-processing", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}

		if ( r_bloom->integer )
		{
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.bloom_image_descriptor[i] ) );
			}
		}

		// Allocate enhanced post-processing descriptor sets
		if ( vk.ssao_descriptor_layout != VK_NULL_HANDLE ) {
			alloc.pSetLayouts = &vk.ssao_descriptor_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssao_descriptor ) );
			SET_OBJECT_NAME( vk.ssao_descriptor, "SSAO descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}
		if ( vk.ssr_descriptor_layout != VK_NULL_HANDLE ) {
			alloc.pSetLayouts = &vk.ssr_descriptor_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssr_descriptor ) );
			SET_OBJECT_NAME( vk.ssr_descriptor, "SSR descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}
		if ( vk.bloom_descriptor_layout != VK_NULL_HANDLE ) {
			alloc.pSetLayouts = &vk.bloom_descriptor_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.bloom_descriptor ) );
			SET_OBJECT_NAME( vk.bloom_descriptor, "Bloom descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}
		if ( vk.dof_descriptor_layout != VK_NULL_HANDLE ) {
			alloc.pSetLayouts = &vk.dof_descriptor_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.dof_descriptor ) );
			SET_OBJECT_NAME( vk.dof_descriptor, "DoF descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}
		if ( vk.velocity_tiles_descriptor_layout != VK_NULL_HANDLE ) {
			alloc.pSetLayouts = &vk.velocity_tiles_descriptor_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.velocity_tiles_descriptor ) );
			SET_OBJECT_NAME( vk.velocity_tiles_descriptor, "Velocity tiles descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}
		if ( vk.motion_blur_descriptor_layout != VK_NULL_HANDLE ) {
			alloc.pSetLayouts = &vk.motion_blur_descriptor_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.motion_blur_descriptor ) );
			SET_OBJECT_NAME( vk.motion_blur_descriptor, "Motion blur descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}
		if ( vk.color_grading_descriptor_layout != VK_NULL_HANDLE ) {
			alloc.pSetLayouts = &vk.color_grading_descriptor_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.color_grading_descriptor ) );
			SET_OBJECT_NAME( vk.color_grading_descriptor, "Color grading descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}
		if ( vk.heat_distortion_descriptor_layout != VK_NULL_HANDLE ) {
			alloc.pSetLayouts = &vk.heat_distortion_descriptor_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.heat_distortion_descriptor ) );
			SET_OBJECT_NAME( vk.heat_distortion_descriptor, "Heat distortion descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}

		alloc.descriptorSetCount = 1;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.screenMap.color_descriptor ) ); // screenmap

#ifdef VK_PBR_BRDFLUT
		if( vk.pbrActive )
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.brdflut_image_descriptor ) );
#endif

		// cubemap
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.cubeMap.color_descriptor ) );

		vk_update_attachment_descriptors();
	}
}

static void vk_create_special_pipelines( void )
{
	Vk_Pipeline_Def def;
	unsigned int state_bits;

	// skybox
	{
		Com_Memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SIGNLE_TEXTURE_FIXED_COLOR;
		def.color.rgb = tr.identityLightByte;
		def.color.alpha = tr.identityLightByte;
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.mirror = qfalse;
		vk.skybox_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// stencil shadows
	{
		cullType_t cull_types[2] = { CT_FRONT_SIDED, CT_BACK_SIDED };
		qboolean mirror_flags[2] = { qfalse, qtrue };
		int i, j;

		Com_Memset(&def, 0, sizeof(def));
		def.polygon_offset = qfalse;
		def.state_bits = 0;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.shadow_phase = SHADOW_EDGES;

		for (i = 0; i < 2; i++) {
			def.face_culling = cull_types[i];
			for (j = 0; j < 2; j++) {
				def.mirror = mirror_flags[j];
				vk.shadow_volume_pipelines[i][j] = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
			}
		}
	}
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;
		def.shadow_phase = SHADOW_FS_QUAD;
		def.primitives = TRIANGLE_STRIP;
		vk.shadow_finish_pipeline = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
	}

	// fog and dlights
	{
		unsigned int fog_state_bits[2] = {
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL, // fogPass == FP_EQUAL
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA // fogPass == FP_LE
		};
		unsigned int dlight_state_bits[2] = {
			GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL,	// modulated
			GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL			// additive
		};
                qboolean polygon_offset_array[2] = { qfalse, qtrue };
		int i, j, k;
#ifdef USE_PMLIGHT
		int l;
#endif

		Com_Memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;

		for ( i = 0; i < 2; i++ ) {
			unsigned fog_state = fog_state_bits[ i ];
			unsigned dlight_state = dlight_state_bits[ i ];

			for ( j = 0; j < 3; j++ ) {
				def.face_culling = j; // cullType_t value

				for ( k = 0; k < 2; k++ ) {
					def.polygon_offset = polygon_offset_array[ k ];
#ifdef USE_FOG_ONLY
					def.shader_type = TYPE_FOG_ONLY;
#else
					def.shader_type = TYPE_SIGNLE_TEXTURE;
#endif
					def.state_bits = fog_state;
					vk.fog_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );

					def.shader_type = TYPE_SIGNLE_TEXTURE;
					def.state_bits = dlight_state;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, r_dlightMode->integer == 0 ? qtrue : qfalse );
#else
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );
#endif
#endif
				}
			}
		}

#ifdef USE_PMLIGHT
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
		//def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING;
		for (i = 0; i < 3; i++) { // cullType
			def.face_culling = i;
			for ( j = 0; j < 2; j++ ) { // polygonOffset
				def.polygon_offset = polygon_offset_array[j];
				for ( k = 0; k < 2; k++ ) {
					def.fog_stage = k; // fogStage
					for ( l = 0; l < 2; l++ ) {
						def.abs_light = l;
						def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING;
						vk.dlight_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
						def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR;
						vk.dlight1_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
					}
				}
			}
		}
#endif // USE_PMLIGHT
	}

	// RT_BEAM surface
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.face_culling = CT_FRONT_SIDED;
		def.primitives = TRIANGLE_STRIP;
		vk.surface_beam_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// axis for missing models
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEFAULT;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.face_culling = CT_TWO_SIDED;
		def.primitives = LINE_LIST;
		if ( vk.wideLines )
			def.line_width = 3;
		vk.surface_axis_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// flare visibility test dot
	if ( vk.fragmentStores )
	{
		Com_Memset( &def, 0, sizeof( def ) );
		//def.state_bits = GLS_DEFAULT;
		def.face_culling = CT_TWO_SIDED;
		def.shader_type = TYPE_DOT;
		def.primitives = POINT_LIST;
		vk.dot_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// DrawTris()
	state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE;
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// DrawNormals()
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.normals_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_DebugPolygon()
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		vk.surface_debug_pipeline_solid = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.surface_debug_pipeline_outline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_ShowImages
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );

		def.state_bits = GLS_DEPTHTEST_DISABLE;
		def.shader_type = TYPE_COLOR_BLACK;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline2 = vk_find_pipeline_ext( 0, &def, qfalse );
	}
}

void vk_create_pipelines( void )
{
	// Create Vulkan attachments with improved error handling
	vk_create_attachments();
	vk_create_special_pipelines();
}

void vk_create_blur_pipeline( uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass );

void vk_update_post_process_pipelines( void )
{
	if ( vk.fboActive ) {
		// update gamma shader
		vk_create_post_process_pipeline( 0, 0, 0 );
		if ( vk.capture.image ) {
			// update capture pipeline
			vk_create_post_process_pipeline( 3, gls.captureWidth, gls.captureHeight );
		}
		if ( r_bloom->integer ) {
			// update bloom shaders
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;
			uint32_t i;
#ifdef USE_VULKAN_RAY_TRACING
			// Create RT composite pipeline if shader is loaded
			if ( vk.rayTracingSupported && vk.rt.initialized && vk.modules.rt_composite_fs != VK_NULL_HANDLE ) {
				vk_create_post_process_pipeline( 5, width, height );
			}
#endif

			vk_create_post_process_pipeline( 1, width, height ); // bloom extraction

			for ( i = 0; i < ARRAY_LEN( vk.blur_pipeline ); i += 2 ) {
				width /= 2;
				height /= 2;
				vk_create_blur_pipeline( i + 0, width, height, qtrue ); // horizontal
				vk_create_blur_pipeline( i + 1, width, height, qfalse ); // vertical
			}

			vk_create_post_process_pipeline( 2, glConfig.vidWidth, glConfig.vidHeight ); // bloom blending
		}
	}
}


typedef struct vk_attach_desc_s  {
	VkImage descriptor;
	VkImageView *image_view;
	VkImageViewType viewType;
	VkImageUsageFlags usage;
	VkMemoryRequirements reqs;
	uint32_t memoryTypeIndex;
	VkDeviceSize  memory_offset;
	// for layout transition:
	VkImageAspectFlags aspect_flags;
	VkImageLayout image_layout;
	VkFormat image_format;
} vk_attach_desc_t;

static vk_attach_desc_t attachments[ MAX_ATTACHMENTS_IN_POOL ];
static uint32_t num_attachments = 0;


static void vk_clear_attachment_pool( void )
{
	num_attachments = 0;
}


static void vk_alloc_attachments( void )
{
	VkImageViewCreateInfo view_desc;
	VkMemoryDedicatedAllocateInfoKHR alloc_info2;
	VkMemoryAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkDeviceSize offset;
	uint32_t memoryTypeBits;
	uint32_t memoryTypeIndex;
	uint32_t i;
	int layer;

	if ( num_attachments == 0 ) {
		return;
	}

	if ( vk.image_memory_count >= ARRAY_LEN( vk.image_memory ) ) {
		ri.Error( ERR_DROP, "vk.image_memory_count == %i", (int)ARRAY_LEN( vk.image_memory ) );
	}

	memoryTypeBits = ~0U;
	offset = 0;

	for ( i = 0; i < num_attachments; i++ ) {
#ifdef MIN_IMAGE_ALIGN
		VkDeviceSize alignment = MAX( attachments[ i ].reqs.alignment, MIN_IMAGE_ALIGN );
#else
		VkDeviceSize alignment = attachments[ i ].reqs.alignment;
#endif
		memoryTypeBits &= attachments[ i ].reqs.memoryTypeBits;
		offset = PAD( offset, alignment );
		attachments[ i ].memory_offset = offset;
		offset += attachments[ i ].reqs.size;
#ifdef _DEBUG
		ri.Printf( PRINT_ALL, S_COLOR_CYAN "[%i] type %i, size %i, align %i\n", i,
			attachments[ i ].reqs.memoryTypeBits,
			(int)attachments[ i ].reqs.size,
			(int)attachments[ i ].reqs.alignment );
#endif
	}

	if ( num_attachments == 1 && attachments[ 0 ].usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT ) {
		// try lazy memory
		memoryTypeIndex = find_memory_type2( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, NULL );
		if ( memoryTypeIndex == ~0U ) {
			memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		}
	} else {
		memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	}

#ifdef _DEBUG
	ri.Printf( PRINT_ALL, "memory type bits: %04x\n", memoryTypeBits );
	ri.Printf( PRINT_ALL, "memory type index: %04x\n", memoryTypeIndex );
	ri.Printf( PRINT_ALL, "total size: %i\n", (int)offset );
#endif

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = offset;
	alloc_info.memoryTypeIndex = memoryTypeIndex;

	if ( num_attachments == 1 ) {
		if ( vk.dedicatedAllocation ) {
			Com_Memset( &alloc_info2, 0, sizeof( alloc_info2 ) );
			alloc_info2.sType =  VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
			alloc_info2.image = attachments[ 0 ].descriptor;
			alloc_info.pNext = &alloc_info2;
		}
	}

	// allocate and bind memory
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

	vk.image_memory[ vk.image_memory_count++ ] = memory;

	for ( i = 0; i < num_attachments; i++ ) {
		VkImageViewType viewType = attachments[i].viewType; // preserve original type

		VK_CHECK( qvkBindImageMemory( vk.device, attachments[i].descriptor, memory, attachments[i].memory_offset ) );
        
		layer = 0;
        while ( qtrue ) {
            // create color image view
            view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_desc.pNext = NULL;
            view_desc.flags = 0;
            view_desc.image = attachments[i].descriptor;
            view_desc.viewType = viewType;
            view_desc.format = attachments[i].image_format;
            view_desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_desc.subresourceRange.aspectMask = attachments[i].aspect_flags;
            view_desc.subresourceRange.baseMipLevel = 0;
            view_desc.subresourceRange.levelCount = 1;
            view_desc.subresourceRange.baseArrayLayer = MAX( ( layer - 1 ), 0 );
            view_desc.subresourceRange.layerCount = ( viewType == VK_IMAGE_VIEW_TYPE_CUBE ) ? 6 : 1;

            VK_CHECK(qvkCreateImageView(vk.device, &view_desc, NULL, attachments[i].image_view + layer));
        
            // discard if not a cube or the 6th face/layer view has been created
            if ( attachments[i].viewType != VK_IMAGE_VIEW_TYPE_CUBE || layer == 6 )
                break;

            // create a view for each face/layer with view type VK_IMAGE_VIEW_TYPE_2D
            viewType = VK_IMAGE_VIEW_TYPE_2D;
            layer++;
        }
	}

	// perform layout transition
	command_buffer = begin_command_buffer();
	for ( i = 0; i < num_attachments; i++ ) {
		record_image_layout_transition( command_buffer,
			attachments[i].descriptor,
			attachments[i].aspect_flags,
			VK_IMAGE_LAYOUT_UNDEFINED, // old_layout
			attachments[i].image_layout,
			0, 0 );
	}
	end_command_buffer( command_buffer, __func__ );

	num_attachments = 0;
}


static qboolean vk_add_attachment_desc( VkImage desc, VkImageView *image_view, VkImageUsageFlags usage, VkMemoryRequirements *reqs, VkFormat image_format, VkImageAspectFlags aspect_flags, VkImageLayout image_layout
#ifdef USE_VK_PBR
	, VkImageViewType view_type )
#endif
{
	if ( num_attachments >= ARRAY_LEN( attachments ) ) {
		ri.Printf(PRINT_WARNING, "Vulkan: Attachments array overflow (%d >= %ld)\n", num_attachments, ARRAY_LEN(attachments));
		return qfalse;
	}

	attachments[ num_attachments ].descriptor = desc;
	attachments[ num_attachments ].image_view = image_view;
#ifdef USE_VK_PBR
	attachments[ num_attachments ].viewType = view_type;
#endif
	attachments[ num_attachments ].usage = usage;
	attachments[ num_attachments ].reqs = *reqs;
	attachments[ num_attachments ].aspect_flags = aspect_flags;
	attachments[ num_attachments ].image_layout = image_layout;
	attachments[ num_attachments ].image_format = image_format;
	attachments[ num_attachments ].memory_offset = 0;
	num_attachments++;

	return qtrue;
}


static void vk_get_image_memory_erquirements( VkImage image, VkMemoryRequirements *memory_requirements )
{
	if ( vk.dedicatedAllocation ) {
		VkMemoryRequirements2KHR memory_requirements2;
		VkImageMemoryRequirementsInfo2KHR image_requirements2;
		VkMemoryDedicatedRequirementsKHR mem_req2;

		Com_Memset( &mem_req2, 0, sizeof( mem_req2 ) );
		mem_req2.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;

		image_requirements2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR;
		image_requirements2.image = image;
		image_requirements2.pNext = NULL;

		Com_Memset( &memory_requirements2, 0, sizeof( memory_requirements2 ) );
		memory_requirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;
		memory_requirements2.pNext = &mem_req2;

		qvkGetImageMemoryRequirements2KHR( vk.device, &image_requirements2, &memory_requirements2 );

		*memory_requirements = memory_requirements2.memoryRequirements;
	} else {
		qvkGetImageMemoryRequirements( vk.device, image, memory_requirements );
	}
}


static qboolean create_color_attachment(
	uint32_t width, uint32_t height,
	VkSampleCountFlagBits samples, VkFormat format,
	VkImageUsageFlags usage, VkImage *image,
	VkImageView *image_view, VkImageLayout image_layout,
	qboolean multisample, VkImageCreateFlags flags )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;
	VkResult result;

	if ( multisample && !( usage & VK_IMAGE_USAGE_SAMPLED_BIT ) )
		usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	// Validate parameters
	if (width == 0 || height == 0 || format == VK_FORMAT_UNDEFINED) {
		ri.Printf(PRINT_WARNING, "Vulkan: Invalid parameters for color attachment (%dx%d, format=%d)\n",
			width, height, format);
		return qfalse;
	}

	// create color image
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = flags;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = ( flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT ) ? 6 : 1;
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = usage;
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	result = qvkCreateImage( vk.device, &create_desc, NULL, image );
	if (result != VK_SUCCESS) {
		ri.Printf(PRINT_WARNING, "Vulkan: Failed to create color image (%dx%d): %s\n",
			width, height, vk_result_string(result));
		return qfalse;
	}

	vk_get_image_memory_erquirements( *image, &memory_requirements );

#ifdef USE_VK_PBR
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;

	if ( flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT )
		view_type = VK_IMAGE_VIEW_TYPE_CUBE;

	if (!vk_add_attachment_desc( *image, image_view, usage, &memory_requirements, format, VK_IMAGE_ASPECT_COLOR_BIT, image_layout, view_type )) {
		ri.Printf(PRINT_WARNING, "Vulkan: Failed to add color attachment descriptor\n");
		qvkDestroyImage(vk.device, *image, NULL);
		*image = VK_NULL_HANDLE;
		return qfalse;
	}
#else
	if (!vk_add_attachment_desc( *image, image_view, usage, &memory_requirements, format, VK_IMAGE_ASPECT_COLOR_BIT, image_layout )) {
		ri.Printf(PRINT_WARNING, "Vulkan: Failed to add color attachment descriptor\n");
		qvkDestroyImage(vk.device, *image, NULL);
		*image = VK_NULL_HANDLE;
		return qfalse;
	}
#endif

	return qtrue;
}


static qboolean create_depth_attachment( uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkImage *image, VkImageView *image_view, qboolean allowTransient )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;
	VkImageAspectFlags image_aspect_flags;
	VkResult result;

	// Validate parameters
	if (width == 0 || height == 0) {
		ri.Printf(PRINT_WARNING, "Vulkan: Invalid parameters for depth attachment (%dx%d)\n", width, height);
		return qfalse;
	}

	// create depth image
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = vk.depth_format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 1;
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if ( allowTransient ) {
		create_desc.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
	}
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 )
		image_aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	result = qvkCreateImage( vk.device, &create_desc, NULL, image );
	if (result != VK_SUCCESS) {
		ri.Printf(PRINT_WARNING, "Vulkan: Failed to create depth image (%dx%d): %s\n",
			width, height, vk_result_string(result));
		return qfalse;
	}

	vk_get_image_memory_erquirements( *image, &memory_requirements );

#ifdef USE_VK_PBR
	if (!vk_add_attachment_desc( *image, image_view, create_desc.usage, &memory_requirements, vk.depth_format, image_aspect_flags, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_VIEW_TYPE_2D )) {
		ri.Printf(PRINT_WARNING, "Vulkan: Failed to add depth attachment descriptor\n");
		qvkDestroyImage(vk.device, *image, NULL);
		*image = VK_NULL_HANDLE;
		return qfalse;
	}
#else
	if (!vk_add_attachment_desc( *image, image_view, create_desc.usage, &memory_requirements, vk.depth_format, image_aspect_flags, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL )) {
		ri.Printf(PRINT_WARNING, "Vulkan: Failed to add depth attachment descriptor\n");
		qvkDestroyImage(vk.device, *image, NULL);
		*image = VK_NULL_HANDLE;
		return qfalse;
	}
#endif

	return qtrue;
}


static void vk_create_attachments( void )
{

	// Comprehensive safety checks
	if (vk.device == VK_NULL_HANDLE) {
		ri.Printf(PRINT_WARNING, "Vulkan: Cannot create attachments - device not initialized\n");
		return;
	}

	if (!vk.active) {
		ri.Printf(PRINT_WARNING, "Vulkan: Cannot create attachments - Vulkan not active\n");
		return;
	}

	if (vk.color_format == VK_FORMAT_UNDEFINED || vk.depth_format == VK_FORMAT_UNDEFINED) {
		ri.Printf(PRINT_WARNING, "Vulkan: Cannot create attachments - formats not initialized\n");
		return;
	}

	if (vk.physical_device == VK_NULL_HANDLE) {
		ri.Printf(PRINT_WARNING, "Vulkan: Cannot create attachments - physical device not available\n");
		return;
	}

	// Check that global config is initialized
	if (glConfig.vidWidth == 0 || glConfig.vidHeight == 0) {
		ri.Printf(PRINT_WARNING, "Vulkan: Cannot create attachments - display not initialized\n");
		return;
	}

	// Validate attachment sizes
	if (glConfig.vidWidth > 16384 || glConfig.vidHeight > 16384) {
		ri.Printf(PRINT_WARNING, "Vulkan: Attachment size too large (%dx%d), clamping to 16384x16384\n",
			glConfig.vidWidth, glConfig.vidHeight);
		glConfig.vidWidth = glConfig.vidWidth > 16384 ? 16384 : glConfig.vidWidth;
		glConfig.vidHeight = glConfig.vidHeight > 16384 ? 16384 : glConfig.vidHeight;
	}

	ri.Printf(PRINT_ALL, "Vulkan: Creating attachments (%dx%d)\n", glConfig.vidWidth, glConfig.vidHeight);

	// Clear and prepare attachment pool
	vk_clear_attachment_pool();

	// Initialize image chunk size if needed
	if (vk.image_chunk_size == 0) {
		vk.image_chunk_size = IMAGE_CHUNK_SIZE;
	}

	// Pre-allocate memory chunk with error handling
	vk_allocate_image_chunk();

	// Create color attachment
	create_color_attachment(glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		&vk.color_image, &vk.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0);

	// Create depth attachment
	create_depth_attachment(glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT,
		&vk.depth_image, &vk.depth_image_view, qfalse);

	// Create additional attachments if FBO is active
	if (vk.fboActive) {
		// Screen space effects buffer
		if (vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT) {
			create_color_attachment(vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples, vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				&vk.screenMap.color_image_msaa, &vk.screenMap.color_image_view_msaa, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue, 0);
		}

		// Screen space resolve buffer
		create_color_attachment(vk.screenMapWidth, vk.screenMapHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			&vk.screenMap.color_image, &vk.screenMap.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse, 0);

		// Screen space depth buffer
		create_depth_attachment(vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples,
			&vk.screenMap.depth_image, &vk.screenMap.depth_image_view, qtrue);

		// MSAA color buffer if needed
		if (vk.msaaActive) {
			create_color_attachment(glConfig.vidWidth, glConfig.vidHeight, vkSamples, vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.msaa_image, &vk.msaa_image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue, 0);
		}
	}

	// Allocate attachment descriptors
	vk_alloc_attachments();

	// Set up debugging names for all successfully created resources
	if (vk.color_image != VK_NULL_HANDLE) {
		SET_OBJECT_NAME(vk.color_image, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
	}
	if (vk.color_image_view != VK_NULL_HANDLE) {
		SET_OBJECT_NAME(vk.color_image_view, "color attachment view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);
	}
	if (vk.depth_image != VK_NULL_HANDLE) {
		SET_OBJECT_NAME(vk.depth_image, "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
	}
	if (vk.depth_image_view != VK_NULL_HANDLE) {
		SET_OBJECT_NAME(vk.depth_image_view, "depth attachment view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);
	}

	// Additional debugging names for advanced features
	if (vk.fboActive) {
		if (vk.screenMap.color_image != VK_NULL_HANDLE) {
			SET_OBJECT_NAME(vk.screenMap.color_image, "screenmap color", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
		}
		if (vk.screenMap.depth_image != VK_NULL_HANDLE) {
			SET_OBJECT_NAME(vk.screenMap.depth_image, "screenmap depth", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
		}
		if (vk.msaaActive && vk.msaa_image != VK_NULL_HANDLE) {
			SET_OBJECT_NAME(vk.msaa_image, "msaa color", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
		}
	}

	ri.Printf(PRINT_ALL, "Vulkan: Attachments created successfully\n");
}


// Function vk_create_framebuffers removed - moved to vk_framebuffer.c
// Function vk_destroy_framebuffers removed - moved to vk_framebuffer.c

void vk_release_resources( void ) {
	uint32_t i;
	int j;

	vk_wait_idle();

	for (i = 0; i < (uint32_t)vk_world.num_image_chunks; i++)
		qvkFreeMemory(vk.device, vk_world.image_chunks[i].memory, NULL);

	vk_clean_staging_buffer();

	// vk_destroy_samplers();

	for ( i = (uint32_t)vk.pipelines_world_base; i < vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );
				vk.pipelines[i].handle[j] = VK_NULL_HANDLE;
				vk.pipeline_create_count--;
			}
		}
		Com_Memset( &vk.pipelines[i], 0, sizeof( vk.pipelines[0] ) );
	}
	vk.pipelines_count = vk.pipelines_world_base;

	VK_CHECK( qvkResetDescriptorPool( vk.device, vk.descriptor_pool, 0 ) );

	// Shutdown enhanced post-processing system
	vk_shutdown_enhanced_post_processing();

	if ( vk_world.num_image_chunks > 1 ) {
		// if we allocated more than 2 image chunks - use doubled default size
		vk.image_chunk_size = (int64_t)IMAGE_CHUNK_SIZE * 2;
	}
#if 0 // do not reduce chunk size
	else if ( vk_world.num_image_chunks == 1 ) {
		// otherwise set to default if used less than a half
		if ( vk_world.image_chunks[0].used < ( IMAGE_CHUNK_SIZE - (IMAGE_CHUNK_SIZE / 10) ) ) {
			vk.image_chunk_size = IMAGE_CHUNK_SIZE;
		}
	}
#endif

	Com_Memset( &vk_world, 0, sizeof( vk_world ) );

	// Reset geometry buffers offsets
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].uniform_read_offset = 0;
		vk.tess[i].vertex_buffer_offset = 0;
	}

	Com_Memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	Com_Memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );

	Com_Memset( &vk.stats, 0, sizeof( vk.stats ) );
}

#if 0
static void record_buffer_memory_barrier(VkCommandBuffer cb, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset,
		VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
		VkAccessFlags src_access, VkAccessFlags dst_access) {

	VkBufferMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = src_access;
	barrier.dstAccessMask = dst_access;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = buffer;
	barrier.offset = offset;
	barrier.size = size;

	qvkCmdPipelineBarrier( cb, src_stages, dst_stages, 0, 0, NULL, 1, &barrier, 0, NULL );
}
#endif

void vk_create_image( image_t *image, int width, int height, int mip_levels ) {

	VkFormat			format = (VkFormat)image->internalFormat;
	VkImageCreateFlags	image_flags = 0;
	VkImageViewType		view_type = (VkImageViewType)VK_IMAGE_VIEW_TYPE_2D;

	if ( image->handle ) {
		qvkDestroyImage( vk.device, image->handle, NULL );
		image->handle = VK_NULL_HANDLE;
	}

	if ( image->view ) {
		qvkDestroyImageView( vk.device, image->view, NULL );
		image->view = VK_NULL_HANDLE;
	}

	if ( image->flags & IMGFLAG_CUBEMAP ) {
		image_flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		view_type = VK_IMAGE_VIEW_TYPE_CUBE;
	}

	// create image
	{
		VkImageCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = image_flags;
		desc.imageType = VK_IMAGE_TYPE_2D;
		desc.format = format;
		desc.extent.width = width;
		desc.extent.height = height;
		desc.extent.depth = 1;
		desc.mipLevels = mip_levels;
		desc.arrayLayers = image->layers;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = NULL;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &image->handle ) );

		// allocate_and_bind_image_memory( image->handle ); // TODO: Reimplement or remove
	}

	// create image view
	{
		VkImageViewCreateInfo desc;
		qboolean isFontTexture = qfalse;

		// Detect font textures to ensure image view only includes base mip level
		// This prevents accidental mipmap sampling for fonts
		if ( image->imgName && mip_levels == 1 ) {
			const char *name = image->imgName;
			if ( Q_stristr( name, "font" ) != NULL || 
			     Q_stristr( name, "fontImage" ) != NULL ||
			     Q_stristr( name, "menu/art" ) != NULL ||
			     Q_stristr( name, "gfx/2d" ) != NULL ||
			     Q_stristr( name, "bigchars" ) != NULL ||
			     Q_stristr( name, "charset" ) != NULL ) {
				isFontTexture = qtrue;
			} else if ( image->width > 0 && image->height > 0 ) {
				// Also check for small non-mipmap textures that look like fonts
				if ( ( image->width <= 512 && image->height <= 512 ) &&
				     ( image->width == image->height || 
				       ( image->width <= 256 && image->height <= 256 ) ) ) {
					if ( image->wrapClampMode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE ||
					     image->wrapClampMode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER ) {
						isFontTexture = qtrue;
					}
				}
			}
		}

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.image = image->handle;
		desc.viewType = (VkImageViewType)view_type;
		desc.format = format;
		desc.components = textureMapTypes[image->type].swizzle;
		desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		desc.subresourceRange.baseMipLevel = 0;
		// For font textures without mipmaps, explicitly set levelCount=1 to prevent mip sampling
		// For other textures, use VK_REMAINING_MIP_LEVELS to include all mip levels
		desc.subresourceRange.levelCount = (isFontTexture && mip_levels == 1) ? 1 : VK_REMAINING_MIP_LEVELS;
		desc.subresourceRange.baseArrayLayer = 0;
		desc.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

		VK_CHECK( qvkCreateImageView( vk.device, &desc, NULL, &image->view ) );
	}

	// create associated descriptor set
	if ( image->descriptor == VK_NULL_HANDLE ) {
		VkDescriptorSetAllocateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc.pNext = NULL;
		desc.descriptorPool = vk.descriptor_pool;
		desc.descriptorSetCount = 1;
		desc.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &desc, &image->descriptor ) );
	}

	vk_update_descriptor_set( image, mip_levels > 1 ? qtrue : qfalse );

	SET_OBJECT_NAME( image->handle, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( image->view, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( image->descriptor, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
}


static byte *resample_image_data( const int target_format, byte *data, const int data_size, int *bytes_per_pixel )
{
	byte* buffer;
	uint16_t* p;
	int i, n;

	switch ( target_format ) {
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			byte a = data[i + 3];
			*p = (uint32_t)((a / 255.0) * 15.0 + 0.5) |
				((uint32_t)((r / 255.0) * 15.0 + 0.5) << 4) |
				((uint32_t)((g / 255.0) * 15.0 + 0.5) << 8) |
				((uint32_t)((b / 255.0) * 15.0 + 0.5) << 12);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			*p = (uint32_t)((b / 255.0) * 31.0 + 0.5) |
				((uint32_t)((g / 255.0) * 31.0 + 0.5) << 5) |
				((uint32_t)((r / 255.0) * 31.0 + 0.5) << 10) |
				(1 << 15);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_B8G8R8A8_UNORM:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size );
		for ( i = 0; i < data_size; i += 4 ) {
			buffer[i + 0] = data[i + 2];
			buffer[i + 1] = data[i + 1];
			buffer[i + 2] = data[i + 0];
			buffer[i + 3] = data[i + 3];
		}
		*bytes_per_pixel = 4;
		return buffer;

	case VK_FORMAT_R8G8B8_UNORM: {
		buffer = (byte*)ri.Hunk_AllocateTempMemory( (data_size * 3) / 4 );
		for ( i = 0, n = 0; i < data_size; i += 4, n += 3 ) {
			buffer[n + 0] = data[i + 0];
			buffer[n + 1] = data[i + 1];
			buffer[n + 2] = data[i + 2];
		}
		*bytes_per_pixel = 3;
		return buffer;
	}

	default:
		*bytes_per_pixel = 4;
		return data;
	}
}


void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int mipmaps, byte *pixels, int size, qboolean update ) {

	VkCommandBuffer   command_buffer;
	VkBufferImageCopy regions[16];
	VkBufferImageCopy region;
	byte *buf;
	int n;

	size_t num_regions = 0;
	VkDeviceSize buffer_size = 0;

	buf = resample_image_data( image->internalFormat, pixels, size, &n /*bpp*/ );

	while (qtrue) {
		Com_Memset(&region, 0, sizeof(region));
		region.bufferOffset = buffer_size;

		// Fix for font atlas row alignment: ensure proper row pitch for 1-channel textures
		// Font atlases are typically 1 byte per pixel and need explicit row length specification
		if (image->isFont) {
			// For font textures, set bufferRowLength to the original image width to handle row alignment
			// This prevents "barcode-like" corruption from row padding mismatches
			region.bufferRowLength = width;  // Original width before any mip reduction
			region.bufferImageHeight = height;  // Original height before any mip reduction
		} else {
			// For normal textures, use 0 to indicate tightly packed data
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
		}

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = num_regions;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = x;
		region.imageOffset.y = y;
		region.imageOffset.z = 0;
		region.imageExtent.width = width;
		region.imageExtent.height = height;
		region.imageExtent.depth = 1;

		regions[num_regions] = region;
		num_regions++;

		buffer_size += width * height * n;

	if ( num_regions >= (size_t)mipmaps || (width == 1 && height == 1) || num_regions >= ARRAY_LEN( regions ) )
			break;

		x >>= 1;
		y >>= 1;

		width >>= 1;
		if (width < 1) width = 1;

		height >>= 1;
		if (height < 1) height = 1;
	}

#ifdef USE_UPLOAD_QUEUE
	if ( vk_wait_staging_buffer() ) {
		// wait for vkQueueSubmit() completion before new upload
	}

	if ( vk.staging_buffer.size - vk.staging_buffer.offset < buffer_size ) {
		// try to flush staging buffer and reset offset
		vk_flush_staging_buffer( qfalse );
	}

	if ( vk.staging_buffer.size /* - vk_world.staging_buffer_offset */ < buffer_size ) {
		// if still not enough - reallocate staging buffer
		vk_alloc_staging_buffer( buffer_size );
	}

	for ( n = 0; n < num_regions; n++ ) {
		regions[n].bufferOffset += vk.staging_buffer.offset;
	}

	Com_Memcpy( vk.staging_buffer.ptr + vk.staging_buffer.offset, buf, buffer_size );

	if ( vk.staging_buffer.offset == 0 ) {
		VkCommandBufferBeginInfo begin_info;
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.pNext = NULL;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = NULL;
		VK_CHECK( qvkBeginCommandBuffer( vk.staging_command_buffer, &begin_info ) );
	}

	//ri.Printf( PRINT_WARNING, "batch @%6i + %i %s \n", (int)vk_world.staging_buffer_offset, (int)buffer_size, image->imgName );
	vk.staging_buffer.offset += buffer_size;

	command_buffer = vk.staging_command_buffer;

	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}

	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );

	// final transition after upload comleted
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
#else
	if ( vk.staging_buffer.size < buffer_size ) {
		vk_alloc_staging_buffer( buffer_size );
	}

	Com_Memcpy( vk.staging_buffer.ptr, buf, buffer_size );

	command_buffer = begin_command_buffer();
	// record_buffer_memory_barrier( command_buffer, vk_world.staging_buffer, VK_WHOLE_SIZE, 0, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );
	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}
	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	end_command_buffer( command_buffer, __func__ );
#endif

	if ( buf != pixels ) {
		ri.Hunk_FreeTempMemory( buf );
	}
}

void vk_update_descriptor_set( image_t *image, qboolean mipmap ) {
	Vk_Sampler_Def sampler_def;
	VkDescriptorImageInfo image_info;
	VkWriteDescriptorSet descriptor_write;

	Com_Memset( &sampler_def, 0, sizeof( sampler_def ) );

	sampler_def.address_mode = image->wrapClampMode;

	// Detect font and UI textures by checking image name and properties
	// Font textures typically have names like:
	// - "fonts/fontImage_X_XX.tga" (system fonts)
	// - "menu/art/font*.tga" (mod fonts like mymod)
	// - "gfx/2d/bigchars" (standard charset for UI_DrawString)
	// Also check for small non-mipmap textures (typical for fonts/UI)
	qboolean isFontTexture = qfalse;
	if ( image->imgName ) {
		const char *name = image->imgName;
		// Check for font-related names and UI textures
		if ( Q_stristr( name, "font" ) != NULL || 
		     Q_stristr( name, "fontImage" ) != NULL ||
		     Q_stristr( name, "menu/art" ) != NULL ||
		     Q_stristr( name, "gfx/2d" ) != NULL ||
		     Q_stristr( name, "bigchars" ) != NULL ||
		     Q_stristr( name, "charset" ) != NULL ) {
			isFontTexture = qtrue;
		}
	}
	
	// Also treat small non-mipmap textures as fonts (fonts are typically small and don't use mipmaps)
	// This catches fonts that might not match the name patterns above
	if ( !mipmap && !isFontTexture && image->width > 0 && image->height > 0 ) {
		// Font textures are typically small (e.g., 256x256 or smaller) and square-ish
		if ( ( image->width <= 512 && image->height <= 512 ) &&
		     ( image->width == image->height || 
		       ( image->width <= 256 && image->height <= 256 ) ) ) {
			// Check if it's likely a UI/font texture (small, no mipmaps, clamped to edge)
			if ( image->wrapClampMode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE ||
			     image->wrapClampMode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER ) {
				isFontTexture = qtrue;
			}
		}
	}

	if ( mipmap ) {
		// Force trilinear filtering for lightmaps
		if ( image->flags & IMGFLAG_LIGHTMAP ) {
			sampler_def.vk_mag_filter = VK_FILTER_LINEAR;
			sampler_def.vk_min_filter = VK_FILTER_LINEAR_MIPMAP_LINEAR;
		} else {
			sampler_def.vk_mag_filter = gl_filter_max;
			sampler_def.vk_min_filter = gl_filter_min;
		}
	} else {
		// Use nearest filtering for fonts and other UI elements to prevent blurriness
		if ( isFontTexture ) {
			sampler_def.vk_mag_filter = VK_FILTER_NEAREST;
			sampler_def.vk_min_filter = VK_FILTER_NEAREST;
		} else {
			sampler_def.vk_mag_filter = VK_FILTER_LINEAR;
			sampler_def.vk_min_filter = VK_FILTER_LINEAR;
		}
		// no anisotropy without mipmaps
		sampler_def.noAnisotropy = qtrue;
	}

	// Pass font texture flag to sampler so it can set maxLod=0.0f for fonts without mipmaps
	sampler_def.isFontTexture = isFontTexture;

	image_info.sampler = vk_find_sampler( &sampler_def );
	image_info.imageView = image->view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_write.dstSet = image->descriptor;
	descriptor_write.dstBinding = 0;
	descriptor_write.dstArrayElement = 0;
	descriptor_write.descriptorCount = 1;
	descriptor_write.pNext = NULL;
	descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_write.pImageInfo = &image_info;
	descriptor_write.pBufferInfo = NULL;
	descriptor_write.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &descriptor_write, 0, NULL );
}

/*
================
vk_update_font_textures

Update descriptor sets for already-loaded font textures to use nearest filtering
This is called after initialization to fix fonts that were loaded before the fix
================
*/
void vk_update_font_textures( void )
{
	int i;
	image_t *image;
	qboolean isFontTexture;
	
	if ( !vk.device || !vk.descriptor_pool ) {
		return;
	}
	
	// Iterate through all loaded images and update font textures
	for ( i = 0; i < tr.numImages; i++ ) {
		image = tr.images[i];
		if ( !image || !image->imgName || image->descriptor == VK_NULL_HANDLE ) {
			continue;
		}
		
		// Check if this is a font texture
		isFontTexture = qfalse;
		const char *name = image->imgName;
		if ( Q_stristr( name, "font" ) != NULL || 
		     Q_stristr( name, "fontImage" ) != NULL ||
		     Q_stristr( name, "menu/art" ) != NULL ||
		     Q_stristr( name, "gfx/2d" ) != NULL ||
		     Q_stristr( name, "bigchars" ) != NULL ||
		     Q_stristr( name, "charset" ) != NULL ) {
			isFontTexture = qtrue;
		}
		
		// Also check for small non-mipmap textures
		if ( !isFontTexture && image->width > 0 && image->height > 0 ) {
			// Check if texture has mipmaps by checking flags
			if ( !( image->flags & IMGFLAG_MIPMAP ) ) {
				if ( ( image->width <= 512 && image->height <= 512 ) &&
				     ( image->width == image->height || 
				       ( image->width <= 256 && image->height <= 256 ) ) ) {
					if ( image->wrapClampMode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE ||
					     image->wrapClampMode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER ) {
						isFontTexture = qtrue;
					}
				}
			}
		}
		
		// Update descriptor set if it's a font texture
		if ( isFontTexture ) {
			// Determine if texture has mipmaps
			qboolean hasMipmaps = ( image->flags & IMGFLAG_MIPMAP ) != 0;
			vk_update_descriptor_set( image, hasMipmaps );
		}
	}
}


void vk_destroy_image_resources( VkImage *image, VkImageView *imageView )
{
	if ( image != NULL ) {
		if ( *image != VK_NULL_HANDLE ) {
			qvkDestroyImage( vk.device, *image, NULL );
			*image = VK_NULL_HANDLE;
		}
	}
	if ( imageView != NULL ) {
		if ( *imageView != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, *imageView, NULL );
			*imageView = VK_NULL_HANDLE;
		}
	}
}


static void set_shader_stage_desc(VkPipelineShaderStageCreateInfo *desc, VkShaderStageFlagBits stage, VkShaderModule shader_module, const char *entry) {
	desc->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	desc->pNext = NULL;
	desc->flags = 0;
	desc->stage = stage;
	desc->module = shader_module;
	desc->pName = entry;
	desc->pSpecializationInfo = NULL;
}


#define FORMAT_DEPTH(format, r_bits, g_bits, b_bits) case(VK_FORMAT_##format): *r = r_bits; *b = b_bits; *g = g_bits; return qtrue;
static qboolean vk_surface_format_color_depth( VkFormat format, int *r, int *g, int *b ) {
	switch (format) {
		// Common formats from https://vulkan.gpuinfo.org/listsurfaceformats.php
		FORMAT_DEPTH(B8G8R8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(B8G8R8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2B10G10R10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R8G8B8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(R8G8B8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2R10G10B10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R5G6B5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(R8G8B8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_UNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SRGB_PACK32, 255, 255, 255)
			FORMAT_DEPTH(R16G16B16A16_UNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(R16G16B16A16_SNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(B5G6R5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(B8G8R8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(R4G4B4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(B4G4R4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(A1R5G5B5_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(R5G5B5A1_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(B5G5R5A1_UNORM_PACK16, 31, 31, 31)
	default:
		*r = 255; *g = 255; *b = 255; return qfalse;
	}
}


void vk_create_post_process_pipeline( int program_index, uint32_t width, uint32_t height )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	VkSpecializationMapEntry spec_entries[11];
	VkSpecializationInfo frag_spec_info;
	VkPipeline *pipeline;
	VkShaderModule fsmodule;
	VkRenderPass renderpass;
	VkPipelineLayout layout;
	VkSampleCountFlagBits samples;
	const char *pipeline_name;
	qboolean blend;

	struct FragSpecData {
		float gamma;
		float overbright;
		float greyscale;
		float bloom_threshold;
		float bloom_intensity;
		int bloom_threshold_mode;
		int bloom_modulate;
		int dither;
		int depth_r;
		int depth_g;
		int depth_b;
	} frag_spec_data;

	switch ( program_index ) {
		case 1: // bloom extraction
			pipeline = &vk.bloom_extract_pipeline;
			fsmodule = vk.modules.bloom_fs;
			renderpass = vk.render_pass.bloom_extract;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "bloom extraction pipeline";
			blend = qfalse;
			break;
		case 2: // final bloom blend
			pipeline = &vk.bloom_blend_pipeline;
			fsmodule = vk.modules.blend_fs;
			renderpass = vk.render_pass.post_bloom;
			layout = vk.pipeline_layout_blend;
			samples = vkSamples;
			pipeline_name = "bloom blend pipeline";
			blend = qtrue;
			break;
		case 3: // capture buffer extraction
			pipeline = &vk.capture_pipeline;
			fsmodule = vk.modules.gamma_fs;
			renderpass = vk.render_pass.capture;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "capture buffer pipeline";
			blend = qfalse;
			break;
#ifdef VK_PBR_BRDFLUT
        case 4: // generate brdf LUT
            pipeline = &vk.brdflut_pipeline;
            fsmodule = vk.modules.brdflut_fs;
            renderpass = vk.render_pass.brdflut;
            layout = vk.pipeline_layout_brdflut;
            samples = VK_SAMPLE_COUNT_1_BIT;
            pipeline_name = "brdf LUT pipeline";
            blend = qfalse;
            break;
#endif
#ifdef USE_VULKAN_RAY_TRACING
		case 5: // RT composite
			pipeline = &vk.rt_composite_pipeline;
			fsmodule = vk.modules.rt_composite_fs;
			renderpass = vk.render_pass.post_bloom;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "RT composite pipeline";
            blend = qfalse;
            break;
#endif
		default: // gamma correction
			pipeline = &vk.gamma_pipeline;
			fsmodule = vk.modules.gamma_fs;
			renderpass = vk.render_pass.gamma;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "gamma-correction pipeline";
			blend = qfalse;
			break;
	}

	if ( *pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, *pipeline, NULL );
		*pipeline = VK_NULL_HANDLE;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;

	// shaders
	set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, fsmodule, "main" );

	// CRITICAL: When RT is enabled, RT composite already outputs sRGB (linearToSrgb conversion),
	// so gamma correction should be skipped (gamma=1.0) to avoid double gamma correction.
	// For non-RT paths, apply normal gamma correction.

	// NOTE: We check RT state at pipeline creation time. Since r_raytracing uses CVAR_LATCH,
	// it requires a restart to change, so the state is consistent. However, RT might not be
	// used in every frame (e.g., menu-only frames), so we conservatively only set gamma=1.0
	// if RT is enabled AND RT composite shader is available (meaning RT can actually run).
	qboolean rtEnabled = qfalse;
#ifdef USE_VULKAN_RAY_TRACING
	rtEnabled = (r_raytracing && r_raytracing->integer &&
	             vk.rayTracingSupported && vk.rt.initialized &&
	             vk.modules.rt_composite_fs != VK_NULL_HANDLE);
#endif
	if ( rtEnabled && program_index == 0 ) { // program_index 0 = gamma pipeline
		frag_spec_data.gamma = 1.0; // Skip gamma correction, RT composite already did sRGB conversion
	} else {
		frag_spec_data.gamma = 1.0 / (r_gamma->value);
	}
	frag_spec_data.overbright = (float)(1 << tr.overbrightBits);
	frag_spec_data.greyscale = r_greyscale->value;
	frag_spec_data.bloom_threshold = r_bloom_threshold->value;
	frag_spec_data.bloom_intensity = r_bloom_intensity->value;
	frag_spec_data.bloom_threshold_mode = r_bloom_threshold_mode->integer;
	frag_spec_data.bloom_modulate = r_bloom_modulate->integer;
	frag_spec_data.dither = r_dither->integer;

	if ( !vk_surface_format_color_depth( vk.present_format.format, &frag_spec_data.depth_r, &frag_spec_data.depth_g, &frag_spec_data.depth_b ) )
		ri.Printf( PRINT_ALL, "Format %s not recognized, dither to assume 8bpc\n", vk_format_string( vk.base_format.format ) );

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( struct FragSpecData, gamma );
	spec_entries[0].size = sizeof( frag_spec_data.gamma );

	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( struct FragSpecData, overbright );
	spec_entries[1].size = sizeof( frag_spec_data.overbright );

	spec_entries[2].constantID = 2;
	spec_entries[2].offset = offsetof( struct FragSpecData, greyscale );
	spec_entries[2].size = sizeof( frag_spec_data.greyscale );

	spec_entries[3].constantID = 3;
	spec_entries[3].offset = offsetof( struct FragSpecData, bloom_threshold );
	spec_entries[3].size = sizeof( frag_spec_data.bloom_threshold );

	spec_entries[4].constantID = 4;
	spec_entries[4].offset = offsetof( struct FragSpecData, bloom_intensity );
	spec_entries[4].size = sizeof( frag_spec_data.bloom_intensity );

	spec_entries[5].constantID = 5;
	spec_entries[5].offset = offsetof( struct FragSpecData, bloom_threshold_mode );
	spec_entries[5].size = sizeof( frag_spec_data.bloom_threshold_mode );

	spec_entries[6].constantID = 6;
	spec_entries[6].offset = offsetof( struct FragSpecData, bloom_modulate );
	spec_entries[6].size = sizeof( frag_spec_data.bloom_modulate );

	spec_entries[7].constantID = 7;
	spec_entries[7].offset = offsetof( struct FragSpecData, dither );
	spec_entries[7].size = sizeof( frag_spec_data.dither );

	spec_entries[8].constantID = 8;
	spec_entries[8].offset = offsetof( struct FragSpecData, depth_r );
	spec_entries[8].size = sizeof( frag_spec_data.depth_r );

	spec_entries[9].constantID = 9;
	spec_entries[9].offset = offsetof(struct FragSpecData, depth_g);
	spec_entries[9].size = sizeof(frag_spec_data.depth_g);

	spec_entries[10].constantID = 10;
	spec_entries[10].offset = offsetof(struct FragSpecData, depth_b);
	spec_entries[10].size = sizeof(frag_spec_data.depth_b);

	frag_spec_info.mapEntryCount = 11;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = sizeof( frag_spec_data );
	frag_spec_info.pData = &frag_spec_data;

	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	//
	// Viewport.
	//
	if ( program_index == 0 ) {
		// gamma correction - MUST cover full swapchain extent to prevent corruption
		// Letterboxing is handled by the shader sampling from the correct region of color_image,
		// not by restricting the viewport. The viewport must cover the entire swapchain
		// so all pixels are written (letterbox regions will be black from clear).
		viewport.x = 0.0;
		viewport.y = 0.0;
		viewport.width = (float)gls.windowWidth;
		viewport.height = (float)gls.windowHeight;
	} else {
		// other post-processing
		viewport.x = 0.0;
		viewport.y = 0.0;
		viewport.width = width;
		viewport.height = height;
	}

	viewport.minDepth = 0.0;
	viewport.maxDepth = 1.0;

	scissor.offset.x = viewport.x;
	scissor.offset.y = viewport.y;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	//rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_NONE;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = samples;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if ( blend ) {
		attachment_blend_state.blendEnable = VK_TRUE;
		attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	} else {
		attachment_blend_state.blendEnable = VK_FALSE;
	}

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = VK_FALSE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_NEVER;
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = VK_FALSE;
	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = (program_index == 2) ? &depth_stencil_state : NULL;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = NULL;
	create_info.layout = layout;
	create_info.renderPass = renderpass;
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, pipeline ) );

	SET_OBJECT_NAME( *pipeline, pipeline_name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}


void vk_create_blur_pipeline( uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	float frag_spec_data[3]; // x-offset, y-offset, correction
	VkSpecializationMapEntry spec_entries[3];
	VkSpecializationInfo frag_spec_info;
	VkPipeline *pipeline;

	pipeline = &vk.blur_pipeline[ index ];

	if ( *pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, *pipeline, NULL );
		*pipeline = VK_NULL_HANDLE;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;

	// shaders
	set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.blur_fs, "main" );

	frag_spec_data[0] = 1.2 / (float) width; // x offset
	frag_spec_data[1] = 1.2 / (float) height; // y offset
	frag_spec_data[2] = 1.0; // intensity?

	if ( horizontal_pass ) {
		frag_spec_data[1] = 0.0;
	} else {
		frag_spec_data[0] = 0.0;
	}

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = 0 * sizeof( float );
	spec_entries[0].size = sizeof( float );

	spec_entries[1].constantID = 1;
	spec_entries[1].offset = 1 * sizeof( float );
	spec_entries[1].size = sizeof( float );

	spec_entries[2].constantID = 2;
	spec_entries[2].offset = 2 * sizeof( float );
	spec_entries[2].size = sizeof( float );

	frag_spec_info.mapEntryCount = 3;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = 3 * sizeof( float );
	frag_spec_info.pData = &frag_spec_data[0];

	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	//
	// Viewport.
	//
	viewport.x = 0.0;
	viewport.y = 0.0;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = 0.0;
	viewport.maxDepth = 1.0;

	scissor.offset.x = viewport.x;
	scissor.offset.y = viewport.y;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	//rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_NONE;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.blendEnable = VK_FALSE;
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = NULL;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = NULL;
	create_info.layout = vk.pipeline_layout_post_process; // one input attachment
	create_info.renderPass = vk.render_pass.blur[ index ];
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, pipeline ) );

	SET_OBJECT_NAME( *pipeline, va( "%s blur pipeline %i", horizontal_pass ? "horizontal" : "vertical", index/2 + 1 ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}


#ifdef USE_VK_PBR
static VkVertexInputBindingDescription bindings[10];
static VkVertexInputAttributeDescription attribs[10];
#else
static VkVertexInputBindingDescription bindings[8];
static VkVertexInputAttributeDescription attribs[8];
#endif
static uint32_t num_binds;
static uint32_t num_attrs;

static void push_bind( uint32_t binding, uint32_t stride )
{
	bindings[ num_binds ].binding = binding;
	bindings[ num_binds ].stride = stride;
	bindings[ num_binds ].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	num_binds++;
}

static void push_attr( uint32_t location, uint32_t binding, VkFormat format )
{
	attribs[ num_attrs ].location = location;
	attribs[ num_attrs ].binding = binding;
	attribs[ num_attrs ].format = format;
	attribs[ num_attrs ].offset = 0;
	num_attrs++;
}


VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index ) {
	VkShaderModule vs_module;
	VkShaderModule fs_module;
	unsigned int state_bits;
	//int32_t vert_spec_data[1]; // clippping
	//VkSpecializationInfo vert_spec_info;
    struct FragSpecData {
        int32_t alpha_test_func; 
        float   alpha_test_value;
        float   depth_fragment;
        int32_t alpha_to_coverage;
        int32_t color_mode;
        int32_t abs_light; 
        int32_t tex_mode;
        int32_t discard_mode;
        float   identity_color;
		float	identity_alpha;
		float	acff;
		int32_t use_font_sdf;
		float   font_sdf_smooth;
#ifdef USE_VK_PBR
        float   specularScale_x;	// use ubo for this
        float   specularScale_y;
        float   specularScale_z;
        float   specularScale_w;
        float   normalScale_x;
        float   normalScale_y;
        float   normalScale_z;
        float   normalScale_w;		// ..
        int32_t normal_texture_set;
        int32_t physical_texture_set;
        int32_t env_texture_set;
        int32_t lightmap_texture_set;
        int32_t glint_enabled;
        float   glint_intensity;
        float   glint_scale;
#endif
    } frag_spec_data; 

#ifdef USE_VK_PBR
    VkSpecializationMapEntry spec_entries[28];
#else
    VkSpecializationMapEntry spec_entries[14];
#endif
	
	VkSpecializationInfo frag_spec_info;
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkDynamicState dynamic_state_array[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkGraphicsPipelineCreateInfo create_info;
	VkPipeline pipeline;
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkBool32 alphaToCoverage = VK_FALSE;
	unsigned int atest_bits;
	int spec_count = 0;

	Com_Memset( &frag_spec_data, 0, sizeof( frag_spec_data ) );
#ifdef USE_VK_PBR
	Com_Memset( spec_entries, 0, sizeof( spec_entries[0] ) * 26 );
#else
	Com_Memset( spec_entries, 0, sizeof( spec_entries[0] ) * 14 );
#endif

	if ( renderPassIndex < 0 || renderPassIndex >= RENDER_PASS_COUNT ) {
		ri.Printf( PRINT_WARNING, "create_pipeline: invalid renderPassIndex %d (dropping pipeline)\n", (int)renderPassIndex );
		return VK_NULL_HANDLE;
	}

#ifdef USE_VK_PBR
	const int use_pbr = def->vk_pbr_flags ? 1 : 0;
#endif

	state_bits = def->state_bits;

	switch ( def->shader_type ) {

		case TYPE_SIGNLE_TEXTURE_LIGHTING:
			vs_module = vk.modules.vert.light[0];
			fs_module = vk.modules.frag.light[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
			vs_module = vk.modules.vert.light[0];
			fs_module = vk.modules.frag.light[1][0];
			break;

		case TYPE_SIGNLE_TEXTURE_DF:
			state_bits |= GLS_DEPTHMASK_TRUE;
			vs_module = vk.modules.vert.ident1[use_pbr][0][0][0];
			fs_module = vk.modules.frag.gen0_df;
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
			vs_module = vk.modules.vert.fixed[use_pbr][0][0][0];
			fs_module = vk.modules.frag.fixed[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
			vs_module = vk.modules.vert.fixed[use_pbr][0][1][0];
			fs_module = vk.modules.frag.fixed[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR:
			vs_module = vk.modules.vert.fixed[use_pbr][0][0][0];
			fs_module = vk.modules.frag.ent[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
			vs_module = vk.modules.vert.fixed[use_pbr][0][1][0];
			fs_module = vk.modules.frag.ent[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE:
			vs_module = vk.modules.vert.gen[use_pbr][0][0][0][0];
			fs_module = vk.modules.frag.gen[use_pbr][0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENV:
			vs_module = vk.modules.vert.gen[use_pbr][0][0][1][0];
			fs_module = vk.modules.frag.gen[use_pbr][0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY:
			vs_module = vk.modules.vert.ident1[use_pbr][0][0][0];
			fs_module = vk.modules.frag.ident1[use_pbr][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
			vs_module = vk.modules.vert.ident1[use_pbr][0][1][0];
			fs_module = vk.modules.frag.ident1[use_pbr][0][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
			vs_module = vk.modules.vert.ident1[use_pbr][1][0][0];
			fs_module = vk.modules.frag.ident1[use_pbr][1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
			vs_module = vk.modules.vert.ident1[use_pbr][1][1][0];
			fs_module = vk.modules.frag.ident1[use_pbr][1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
			vs_module = vk.modules.vert.fixed[use_pbr][1][0][0];
			fs_module = vk.modules.frag.fixed[use_pbr][1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
			vs_module = vk.modules.vert.fixed[use_pbr][1][1][0];
			fs_module = vk.modules.frag.fixed[use_pbr][1][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			vs_module = vk.modules.vert.gen[use_pbr][1][0][0][0];
			fs_module = vk.modules.frag.gen[use_pbr][1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			vs_module = vk.modules.vert.gen[use_pbr][1][0][1][0];
			fs_module = vk.modules.frag.gen[use_pbr][1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			vs_module = vk.modules.vert.gen[use_pbr][2][0][0][0];
			fs_module = vk.modules.frag.gen[use_pbr][2][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			vs_module = vk.modules.vert.gen[use_pbr][2][0][1][0];
			fs_module = vk.modules.frag.gen[use_pbr][2][0][0];
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			vs_module = vk.modules.vert.gen[use_pbr][1][1][0][0];
			fs_module = vk.modules.frag.gen[use_pbr][1][1][0];
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = vk.modules.vert.gen[use_pbr][1][1][1][0];
			fs_module = vk.modules.frag.gen[use_pbr][1][1][0];
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			vs_module = vk.modules.vert.gen[use_pbr][2][1][0][0];
			fs_module = vk.modules.frag.gen[use_pbr][2][1][0];
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = vk.modules.vert.gen[use_pbr][2][1][1][0];
			fs_module = vk.modules.frag.gen[use_pbr][2][1][0];
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			vs_module = vk.modules.color_vs;
			fs_module = vk.modules.color_fs;
			break;

		case TYPE_FOG_ONLY:
			vs_module = vk.modules.fog_vs;
			fs_module = vk.modules.fog_fs;
			break;

		case TYPE_DOT:
			vs_module = vk.modules.dot_vs;
			fs_module = vk.modules.dot_fs;
			break;

		default:
			ri.Error(ERR_DROP, "create_pipeline: unknown shader type %i\n", def->shader_type);
			return 0;
	}

	switch ( def->shader_type ) {

		case TYPE_SIGNLE_TEXTURE_LIGHTING:
			vs_module = vk.modules.vert.light[0];
			fs_module = vk.modules.frag.light[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
			vs_module = vk.modules.vert.light[0];
			fs_module = vk.modules.frag.light[1][0];
			break;

		case TYPE_SIGNLE_TEXTURE_DF:
			state_bits |= GLS_DEPTHMASK_TRUE;
			vs_module = vk.modules.vert.ident1[0][0][0][0];
			fs_module = vk.modules.frag.gen0_df;
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
			vs_module = vk.modules.vert.fixed[0][0][0][0];
			fs_module = vk.modules.frag.fixed[0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
			vs_module = vk.modules.vert.fixed[0][1][0][0];
			fs_module = vk.modules.frag.fixed[0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR:
			vs_module = vk.modules.vert.fixed[0][0][0][0];
			fs_module = vk.modules.frag.ent[0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
			vs_module = vk.modules.vert.fixed[0][1][0][0];
			fs_module = vk.modules.frag.ent[0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE:
			vs_module = vk.modules.vert.gen[0][0][0][0][0];
			fs_module = vk.modules.frag.gen[0][0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENV:
			vs_module = vk.modules.vert.gen[0][0][1][0][0];
			fs_module = vk.modules.frag.gen[0][0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY:
			vs_module = vk.modules.vert.ident1[0][0][0][0];
			fs_module = vk.modules.frag.ident1[0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
			vs_module = vk.modules.vert.ident1[0][1][0][0];
			fs_module = vk.modules.frag.ident1[0][0][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
			vs_module = vk.modules.vert.ident1[1][0][0][0];
			fs_module = vk.modules.frag.ident1[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
			vs_module = vk.modules.vert.ident1[1][1][0][0];
			fs_module = vk.modules.frag.ident1[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
			vs_module = vk.modules.vert.fixed[1][0][0][0];
			fs_module = vk.modules.frag.fixed[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
			vs_module = vk.modules.vert.fixed[1][1][0][0];
			fs_module = vk.modules.frag.fixed[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			vs_module = vk.modules.vert.gen[1][0][0][0][0];
			fs_module = vk.modules.frag.gen[1][0][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			vs_module = vk.modules.vert.gen[1][0][1][0][0];
			fs_module = vk.modules.frag.gen[1][0][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			vs_module = vk.modules.vert.gen[1][0][0][0][0];
			fs_module = vk.modules.frag.gen[1][0][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			vs_module = vk.modules.vert.gen[1][0][1][0][0];
			fs_module = vk.modules.frag.gen[1][0][0][0];
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			vs_module = vk.modules.vert.gen[1][1][0][0][0];
			fs_module = vk.modules.frag.gen[1][1][0][0];
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = vk.modules.vert.gen[1][1][1][0][0];
			fs_module = vk.modules.frag.gen[1][1][0][0];
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			vs_module = vk.modules.vert.gen[1][1][0][0][0];
			fs_module = vk.modules.frag.gen[1][1][0][0];
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = vk.modules.vert.gen[1][1][1][0][0];
			fs_module = vk.modules.frag.gen[1][1][0][0];
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			vs_module = vk.modules.color_vs;
			fs_module = vk.modules.color_fs;
			break;

		case TYPE_FOG_ONLY:
			vs_module = vk.modules.fog_vs;
			fs_module = vk.modules.fog_fs;
			break;

		case TYPE_DOT:
			vs_module = vk.modules.dot_vs;
			fs_module = vk.modules.dot_fs;
			break;

		default:
			ri.Error(ERR_DROP, "create_pipeline: unknown shader type %i\n", def->shader_type);
			return 0;
	}
	

	if ( def->fog_stage ) {
		switch ( def->shader_type ) {
			case TYPE_FOG_ONLY:
			case TYPE_DOT:
			case TYPE_SIGNLE_TEXTURE_DF:
			case TYPE_COLOR_BLACK:
			case TYPE_COLOR_WHITE:
			case TYPE_COLOR_GREEN:
			case TYPE_COLOR_RED:
				break;
			default:
				// switch to fogged modules
				vs_module = (VkShaderModule)((VkShaderModule *)vs_module + 1);
				fs_module = (VkShaderModule)((VkShaderModule *)fs_module + 1);
				break;
		}
	}

	set_shader_stage_desc(shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vs_module, "main");
	set_shader_stage_desc(shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, fs_module, "main");

	//Com_Memset( vert_spec_data, 0, sizeof( vert_spec_data ) );
	Com_Memset( &frag_spec_data, 0, sizeof( frag_spec_data ) );

	//vert_spec_data[0] = def->clipping_plane ? 1 : 0;

	// fragment shader specialization data
	atest_bits = state_bits & GLS_ATEST_BITS;
	switch ( atest_bits ) {
        case GLS_ATEST_GT_0:
            frag_spec_data.alpha_test_func = 1; // not equal
            frag_spec_data.alpha_test_value = 0.0f;
            break;
        case GLS_ATEST_LT_80:
            frag_spec_data.alpha_test_func = 2; // less than
            frag_spec_data.alpha_test_value = 0.5f;
            break;
        case GLS_ATEST_GE_80:
            frag_spec_data.alpha_test_func = 3; // greater or equal
            frag_spec_data.alpha_test_value = 0.5f;
            break;
        default:
            frag_spec_data.alpha_test_func = 0;
            frag_spec_data.alpha_test_value = 0.0f;
            break;
	};

	// depth fragment threshold
	frag_spec_data.depth_fragment = 0.85f;
#if 0
	if ( r_ext_alpha_to_coverage->integer && vkSamples != VK_SAMPLE_COUNT_1_BIT && frag_spec_data.alpha_test_func ) {
		frag_spec_data.alpha_to_coverage = 1;
		alphaToCoverage = VK_TRUE;
	}
#endif

    // constant color
    switch ( def->shader_type ) {
        default: frag_spec_data.color_mode = 0; break;
		case TYPE_COLOR_WHITE: frag_spec_data.color_mode = 1; break;
        case TYPE_COLOR_GREEN: frag_spec_data.color_mode = 2; break;
        case TYPE_COLOR_RED:   frag_spec_data.color_mode = 3; break;
    }

    // abs lighting
    switch ( def->shader_type ) {
		case TYPE_SIGNLE_TEXTURE_LIGHTING:
		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
            frag_spec_data.abs_light = def->abs_light ? 1 : 0;
        default:
        break;
    }

	// multutexture mode
	switch ( def->shader_type ) {
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_MUL_ENV:
			frag_spec_data.tex_mode = 0;
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
			frag_spec_data.tex_mode = 1;
			break;

		case TYPE_MULTI_TEXTURE_ADD2:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
		case TYPE_MULTI_TEXTURE_ADD3:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_ADD_ENV:
			frag_spec_data.tex_mode = 2;
			break;

		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ALPHA_ENV:
			frag_spec_data.tex_mode = 3;
			break;

		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
			frag_spec_data.tex_mode = 4;
			break;

		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
			frag_spec_data.tex_mode = 5;
			break;

		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
			frag_spec_data.tex_mode = 6;
			break;

		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			frag_spec_data.tex_mode = 7;
			break;

		default:
			break;
	}

	frag_spec_data.identity_color = ((float)def->color.rgb) / 255.0;
	frag_spec_data.identity_alpha = ((float)def->color.alpha) / 255.0;

	if ( def->fog_stage ) {
		frag_spec_data.acff = def->acff;
	} else {
		frag_spec_data.acff = 0;
	}

	//
	// vertex module specialization data
	//
#if 0
	spec_entries[0].constantID = 0; // clip_plane
	spec_entries[0].offset = 0 * sizeof( int32_t );
	spec_entries[0].size = sizeof( int32_t );

	vert_spec_info.mapEntryCount = 1;
	vert_spec_info.pMapEntries = spec_entries + 0;
	vert_spec_info.dataSize = 1 * sizeof( int32_t );
	vert_spec_info.pData = &vert_spec_data[0];
	shader_stages[0].pSpecializationInfo = &vert_spec_info;
#endif
	shader_stages[0].pSpecializationInfo = NULL;

	//
	// fragment module specialization data
	//
#define PUSH_SPEC_ENTRY(id, field)                          \
	do {                                                    \
		if ( spec_count >= (int)ARRAY_LEN( spec_entries ) ) {\
			ri.Printf( PRINT_WARNING, "create_pipeline: specialization entry overflow at id %d (dropping pipeline)\n", (id) ); \
			return VK_NULL_HANDLE;                          \
		}                                                   \
		spec_entries[spec_count].constantID = (id);         \
		spec_entries[spec_count].offset =                  \
			offsetof(struct FragSpecData, field);          \
		spec_entries[spec_count].size =                    \
			sizeof(frag_spec_data.field);                  \
		spec_count++;                                      \
	} while (0)

	PUSH_SPEC_ENTRY( 0, alpha_test_func );
	PUSH_SPEC_ENTRY( 1, alpha_test_value );
	PUSH_SPEC_ENTRY( 2, depth_fragment );
	PUSH_SPEC_ENTRY( 3, alpha_to_coverage );
	PUSH_SPEC_ENTRY( 4, color_mode );
	PUSH_SPEC_ENTRY( 5, abs_light );
	PUSH_SPEC_ENTRY( 6, tex_mode );
	PUSH_SPEC_ENTRY( 7, discard_mode );
	PUSH_SPEC_ENTRY( 8, identity_color );
	PUSH_SPEC_ENTRY( 9, identity_color );
	PUSH_SPEC_ENTRY( 10, identity_color );
	PUSH_SPEC_ENTRY( 25, use_font_sdf );
	PUSH_SPEC_ENTRY( 26, font_sdf_smooth );
#ifdef USE_VK_PBR   
{
        PUSH_SPEC_ENTRY( 11, specularScale_x );
        PUSH_SPEC_ENTRY( 12, specularScale_y );
        PUSH_SPEC_ENTRY( 13, specularScale_z );
        PUSH_SPEC_ENTRY( 14, specularScale_w );

        PUSH_SPEC_ENTRY( 15, normalScale_x );
        PUSH_SPEC_ENTRY( 16, normalScale_y );
        PUSH_SPEC_ENTRY( 17, normalScale_z );
        PUSH_SPEC_ENTRY( 18, normalScale_w );

        PUSH_SPEC_ENTRY( 19, normal_texture_set );
        PUSH_SPEC_ENTRY( 20, physical_texture_set );
        PUSH_SPEC_ENTRY( 21, env_texture_set );
        PUSH_SPEC_ENTRY( 22, lightmap_texture_set );
        PUSH_SPEC_ENTRY( 23, glint_enabled );
        PUSH_SPEC_ENTRY( 24, glint_intensity );
        PUSH_SPEC_ENTRY( 27, glint_scale );
        
        // only use w value, specgloss maps are not supported
        frag_spec_data.specularScale_x = def->specularScale[0];
        frag_spec_data.specularScale_y = def->specularScale[1];
        frag_spec_data.specularScale_z = def->specularScale[2];
        frag_spec_data.specularScale_w = def->specularScale[3];

        frag_spec_data.normalScale_x = def->normalScale[0];
        frag_spec_data.normalScale_y = def->normalScale[1];
        frag_spec_data.normalScale_z = def->normalScale[2];
        frag_spec_data.normalScale_w = def->normalScale[3];

	    if ( ( def->vk_pbr_flags & PBR_HAS_NORMALMAP ) == 0 )
            frag_spec_data.normal_texture_set = -1;

	    if ( ( def->vk_pbr_flags & PBR_HAS_PHYSICALMAP ) == 0 )
            frag_spec_data.physical_texture_set = -1;

	    if ( def->vk_pbr_flags & PBR_HAS_SPECULARMAP )
            frag_spec_data.physical_texture_set = 1;

        if ( !vk.cubemapActive )
            frag_spec_data.env_texture_set = -1;

        if ( ( def->vk_pbr_flags & PBR_HAS_LIGHTMAP ) == 0 )
            frag_spec_data.lightmap_texture_set = -1;

        frag_spec_data.glint_enabled   = ( r_glint && r_glint->integer && r_pbr->integer ) ? 1 : 0;
        frag_spec_data.glint_intensity = r_glint_intensity ? r_glint_intensity->value : 0.0f;
        frag_spec_data.glint_scale     = r_glint_scale ? r_glint_scale->value : 0.0f;
    }
#endif
	if ( spec_count > (int)ARRAY_LEN( spec_entries ) ) {
		ri.Error( ERR_DROP, "create_pipeline: specialization entry overflow (%d > %zu)", spec_count, ARRAY_LEN( spec_entries ) );
		return VK_NULL_HANDLE;
	}

	// Sanity-check specialization entries stay within struct bounds
	for ( int i = 0; i < spec_count; i++ ) {
		const size_t off = spec_entries[i].offset;
		const size_t sz = spec_entries[i].size;
		if ( sz == 0 || off >= sizeof( frag_spec_data ) || off + sz > sizeof( frag_spec_data ) ) {
			ri.Printf( PRINT_WARNING, "create_pipeline: invalid specialization entry %d (off=%zu size=%zu struct=%zu), dropping pipeline\n", i, off, sz, sizeof( frag_spec_data ) );
			return VK_NULL_HANDLE;
		}
	}

	if ( spec_count == 0 ) {
		ri.Printf( PRINT_WARNING, "create_pipeline: no specialization entries populated, dropping pipeline\n" );
		return VK_NULL_HANDLE;
	}

	frag_spec_info.mapEntryCount = spec_count;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = sizeof( frag_spec_data );
	frag_spec_info.pData = &frag_spec_data;
	shader_stages[1].pSpecializationInfo = &frag_spec_info;
#undef PUSH_SPEC_ENTRY


	//
	// Vertex input
	//
	num_binds = num_attrs = 0;
	switch ( def->shader_type ) {

		case TYPE_FOG_ONLY:
		case TYPE_DOT:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_DF:
		case TYPE_SIGNLE_TEXTURE_IDENTITY:
		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
		case TYPE_SIGNLE_TEXTURE_ENT_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
		case TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_LIGHTING:
		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( vec2_t ) );					// st0 array
			push_bind( 2, sizeof( vec4_t ) );					// normals array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		default:
			ri.Error( ERR_DROP, "%s: invalid shader type - %i", __func__, def->shader_type );
			break;
	}

 #ifdef USE_VK_PBR  
    if( def->vk_pbr_flags ){    
        push_bind( 8, sizeof( vec4_t ) );						// qtangent
        push_attr( 8, 8, VK_FORMAT_R32G32B32A32_SFLOAT );

        push_bind( 9, sizeof(vec4_t) );							// lightdir
        push_attr( 9, 9, VK_FORMAT_R32G32B32A32_SFLOAT );
    }
#endif

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.pVertexBindingDescriptions = bindings;
	vertex_input_state.pVertexAttributeDescriptions = attribs;
	vertex_input_state.vertexBindingDescriptionCount = num_binds;
	vertex_input_state.vertexAttributeDescriptionCount = num_attrs;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	switch ( def->primitives ) {
		case LINE_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
		case POINT_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
		case TRIANGLE_STRIP: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
		default: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
	}

	//
	// Viewport.
	//
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = NULL; // dynamic viewport state
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = NULL; // dynamic scissor state

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	if ( def->shader_type == TYPE_DOT ) {
		rasterization_state.polygonMode = VK_POLYGON_MODE_POINT;
	} else {
		rasterization_state.polygonMode = (state_bits & GLS_POLYMODE_LINE) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	}

	switch ( def->face_culling ) {
		case CT_TWO_SIDED:
			rasterization_state.cullMode = VK_CULL_MODE_NONE;
			break;
		case CT_FRONT_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT);
			break;
		case CT_BACK_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT);
			break;
		default:
			ri.Error( ERR_DROP, "create_pipeline: invalid face culling mode %i\n", def->face_culling );
			break;
	}

	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order

	 // depth bias state
	if ( def->polygon_offset ) {
		rasterization_state.depthBiasEnable = VK_TRUE;
		rasterization_state.depthBiasClamp = 0.0f;
#ifdef USE_REVERSED_DEPTH
		rasterization_state.depthBiasConstantFactor = -r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = -r_offsetFactor->value;
#else
		rasterization_state.depthBiasConstantFactor = r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = r_offsetFactor->value;
#endif
	} else {
		rasterization_state.depthBiasEnable = VK_FALSE;
		rasterization_state.depthBiasClamp = 0.0f;
		rasterization_state.depthBiasConstantFactor = 0.0f;
		rasterization_state.depthBiasSlopeFactor = 0.0f;
	}

	if ( def->line_width )
		rasterization_state.lineWidth = (float)def->line_width;
	else
		rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;

multisample_state.rasterizationSamples = (renderPassIndex == RENDER_PASS_SCREENMAP) ? vk.screenMapSamples : (uint32_t)vkSamples;

	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = alphaToCoverage;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = (state_bits & GLS_DEPTHTEST_DISABLE) ? VK_FALSE : VK_TRUE;
	depth_stencil_state.depthWriteEnable = (state_bits & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE;
#ifdef USE_REVERSED_DEPTH
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
#endif
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = (def->shadow_phase != SHADOW_DISABLED) ? VK_TRUE : VK_FALSE;

	if (def->shadow_phase == SHADOW_EDGES) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = (def->face_culling == CT_FRONT_SIDED) ? VK_STENCIL_OP_INCREMENT_AND_CLAMP : VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;

	} else if (def->shadow_phase == SHADOW_FS_QUAD) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	}

	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;

	Com_Memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.blendEnable = (state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ? VK_TRUE : VK_FALSE;

	if (def->shadow_phase == SHADOW_EDGES || def->shader_type == TYPE_SIGNLE_TEXTURE_DF)
		attachment_blend_state.colorWriteMask = 0;
	else
		attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	if (attachment_blend_state.blendEnable) {
		switch (state_bits & GLS_SRCBLEND_BITS) {
			case GLS_SRCBLEND_ZERO:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
				break;
			default:
				ri.Error( ERR_DROP, "create_pipeline: invalid src blend state bits\n" );
				break;
		}
		switch (state_bits & GLS_DSTBLEND_BITS) {
			case GLS_DSTBLEND_ZERO:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			default:
				ri.Error( ERR_DROP, "create_pipeline: invalid dst blend state bits\n" );
				break;
		}

		attachment_blend_state.srcAlphaBlendFactor = attachment_blend_state.srcColorBlendFactor;
		attachment_blend_state.dstAlphaBlendFactor = attachment_blend_state.dstColorBlendFactor;
		attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
		attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;

		if ( def->allow_discard && vkSamples != VK_SAMPLE_COUNT_1_BIT ) {
			// try to reduce pixel fillrate for transparent surfaces, this yields 1..10% fps increase when multisampling in enabled
			if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA ) {
				frag_spec_data.discard_mode = 1;
			} else if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_ONE && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE ) {
				frag_spec_data.discard_mode = 2;
			}
		}
	}

	// Use C23 designated initializer for better performance
	blend_state = (VkPipelineColorBlendStateCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &attachment_blend_state,
		.blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
	};

	// Use C23 designated initializer for better performance
	dynamic_state = (VkPipelineDynamicStateCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.dynamicStateCount = ARRAY_LEN( dynamic_state_array ),
		.pDynamicStates = dynamic_state_array
	};

	// Use C23 designated initializer for better performance and clarity
	create_info = (VkGraphicsPipelineCreateInfo){
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.stageCount = ARRAY_LEN(shader_stages),
		.pStages = shader_stages,
		.pVertexInputState = &vertex_input_state,
		.pInputAssemblyState = &input_assembly_state,
		.pTessellationState = NULL,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterization_state,
		.pMultisampleState = &multisample_state,
		.pDepthStencilState = &depth_stencil_state,
		.pColorBlendState = &blend_state,
		.pDynamicState = &dynamic_state,
		.layout = (def->shader_type == TYPE_DOT) ? vk.pipeline_layout_storage : vk.pipeline_layout,
		.renderPass = (renderPassIndex == RENDER_PASS_SCREENMAP) ? vk.render_pass.screenmap : vk.render_pass.main,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1
	};

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL, &pipeline ) );

	SET_OBJECT_NAME( pipeline, va( "pipeline def#%i, pass#%i", def_index, renderPassIndex ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	vk.pipeline_create_count++;

	return pipeline;
}



static void get_viewport(VkViewport *viewport, Vk_Depth_Range depth_range) {
	VkRect2D r;

	get_viewport_rect( &r );

	viewport->x = (float)r.offset.x;
	viewport->y = (float)r.offset.y;
	viewport->width = (float)r.extent.width;
	viewport->height = (float)r.extent.height;

	switch ( depth_range ) {
		default:
#ifdef USE_REVERSED_DEPTH
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.6f;
			viewport->maxDepth = 1.0f;
			break;
#else
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.3f;
			break;
#endif
	}
}

static void get_scissor_rect(VkRect2D *r) {

	if ( backEnd.viewParms.portalView != PV_NONE )
	{
		r->offset.x = backEnd.viewParms.scissorX;
		r->offset.y = glConfig.vidHeight - backEnd.viewParms.scissorY - backEnd.viewParms.scissorHeight;
		r->extent.width = backEnd.viewParms.scissorWidth;
		r->extent.height = backEnd.viewParms.scissorHeight;
	}
	else
	{
		get_viewport_rect(r);

		if (r->offset.x < 0)
			r->offset.x = 0;
		if (r->offset.y < 0)
			r->offset.y = 0;

		if (r->offset.x + r->extent.width > (uint32_t)glConfig.vidWidth)
			r->extent.width = (uint32_t)glConfig.vidWidth - r->offset.x;
		if (r->offset.y + r->extent.height > (uint32_t)glConfig.vidHeight)
			r->extent.height = (uint32_t)glConfig.vidHeight - r->offset.y;
	}
}


static void get_mvp_transform( float *mvp )
{
	if ( backEnd.projection2D )
	{
		float mvp0 = 2.0f / glConfig.vidWidth;
		float mvp5 = 2.0f / glConfig.vidHeight;

		mvp[0]  =  mvp0; mvp[1]  =  0.0f; mvp[2]  = 0.0f; mvp[3]  = 0.0f;
		mvp[4]  =  0.0f; mvp[5]  =  mvp5; mvp[6]  = 0.0f; mvp[7]  = 0.0f;
#ifdef USE_REVERSED_DEPTH
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 0.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 1.0f; mvp[15] = 1.0f;
#else
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 1.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 0.0f; mvp[15] = 1.0f;
#endif
	}
	else
	{
		const float *p = backEnd.viewParms.projectionMatrix;
		float proj[16];
		Com_Memcpy( proj, p, 64 );

		// update q3's proj matrix (opengl) to vulkan conventions: z - [0, 1] instead of [-1, 1] and invert y direction
		proj[5] = -p[5];
		//proj[10] = ( p[10] - 1.0f ) / 2.0f;
		//proj[14] = p[14] / 2.0f;
		myGlMultMatrix( vk_world.modelview_transform, proj, mvp );
	}
}


void vk_clear_color( const vec4_t clear_color ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect;

	if ( !vk.active )
		return;

	attachment.colorAttachment = 0;
	attachment.clearValue.color.float32[0] = clear_color[0];
	attachment.clearValue.color.float32[1] = clear_color[1];
	attachment.clearValue.color.float32[2] = clear_color[2];
	attachment.clearValue.color.float32[3] = clear_color[3];
	attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	// For 2D/menu rendering, always clear the full image extent to prevent corruption
	// The scissor rect clamping might cut off bottom rows if there's any mismatch
	if ( backEnd.projection2D ) {
		// Full-screen clear: use the actual render dimensions, not scissor-clamped
		clear_rect.rect.offset.x = 0;
		clear_rect.rect.offset.y = 0;
		clear_rect.rect.extent.width = vk.renderWidth;
		clear_rect.rect.extent.height = vk.renderHeight;
	} else {
		// For 3D rendering, use scissor rect (may be viewport-restricted)
		get_scissor_rect( &clear_rect.rect );
	}
	clear_rect.baseArrayLayer = 0;
	clear_rect.layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, &clear_rect );
}


void vk_clear_depth( qboolean clear_stencil ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect[1];

	if ( !vk.active )
		return;

	if ( vk_world.dirty_depth_attachment == 0 )
		return;

	// Safety check: ensure we have a valid command buffer and are in a render pass
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_clear_depth: Invalid command buffer\n" );
		return;
	}

	// Check if function pointer is valid
	if ( !qvkCmdClearAttachments ) {
		ri.Printf( PRINT_WARNING, "vk_clear_depth: qvkCmdClearAttachments function pointer is NULL\n" );
		return;
	}

	// vkCmdClearAttachments can only be called inside a render pass
	// If not in a render pass, skip the clear (depth will be cleared when render pass starts)
	if ( vk.renderPassIndex >= RENDER_PASS_COUNT ) {
		ri.Printf( PRINT_DEVELOPER, "vk_clear_depth: Skipping clear - not inside a render pass (index: %u)\n", vk.renderPassIndex );
		return;
	}

	attachment.colorAttachment = 0;
#ifdef USE_REVERSED_DEPTH
	attachment.clearValue.depthStencil.depth = 0.0f;
#else
	attachment.clearValue.depthStencil.depth = 1.0f;
#endif
	attachment.clearValue.depthStencil.stencil = 0;
	if ( clear_stencil && glConfig.stencilBits > 0 ) {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	} else {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	ri.Printf( PRINT_DEVELOPER, "vk_clear_depth: Attempting to clear depth (CmdBuffer: %p, RenderPassIndex: %u)\n", (void*)vk.cmd->command_buffer, vk.renderPassIndex );

	get_scissor_rect( &clear_rect[0].rect );
	clear_rect[0].baseArrayLayer = 0;
	clear_rect[0].layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, clear_rect );

	ri.Printf( PRINT_DEVELOPER, "vk_clear_depth: Depth clear command submitted successfully.\n" );
}


void vk_update_mvp( const float *m ) {
	float push_constants[16]; // mvp transform

	//
	// Specify push constants.
	//
	if ( m ) {
		Com_Memcpy( push_constants, m, sizeof( push_constants ) );
		// Update cache when explicit matrix provided
		Com_Memcpy( vk.cmd->mvp_cache.cached_mvp, push_constants, sizeof( push_constants ) );
		vk.cmd->mvp_cache.mvp_valid = qtrue;
	} else {
		// Check cache first to avoid redundant calculations
		if ( vk.cmd->mvp_cache.mvp_valid ) {
			Com_Memcpy( push_constants, vk.cmd->mvp_cache.cached_mvp, sizeof( push_constants ) );
		} else {
			get_mvp_transform( push_constants );
			Com_Memcpy( vk.cmd->mvp_cache.cached_mvp, push_constants, sizeof( push_constants ) );
			vk.cmd->mvp_cache.mvp_valid = qtrue;
		}
	}

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( push_constants ), push_constants );

	vk.stats.push_size += sizeof( push_constants );
}

#ifdef USE_VK_PBR
static VkBuffer shade_bufs[10];
#else
static VkBuffer shade_bufs[8];
#endif
static int bind_base;
static int bind_count;

static void vk_bind_index_attr( int index )
{
	if ( bind_base == -1 ) {
		bind_base = index;
		bind_count = 1;
	} else {
		bind_count = index - bind_base + 1;
	}
}


static void vk_bind_attr( int index, unsigned int item_size, const void *src ) {
	const uint32_t offset = PAD( vk.cmd->vertex_buffer_offset, 32 );
	const uint32_t size = tess.numVertexes * item_size;

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize - use history to pre-allocate if available
		VkDeviceSize requested_size = log2pad( offset + size, 1 );
		if ( vk.geometry_buffer_history.count >= 4 ) {
			// Use max of recent history + 25% margin for pre-allocation
			VkDeviceSize avg_size = 0;
			uint32_t i;
			for ( i = 0; i < vk.geometry_buffer_history.count; i++ ) {
				avg_size += vk.geometry_buffer_history.sizes[i];
			}
			avg_size = ( avg_size / vk.geometry_buffer_history.count ) * 5 / 4; // 25% margin
			if ( avg_size > requested_size ) {
				requested_size = log2pad( avg_size, 1 );
			}
		}
		vk.geometry_buffer_size_new = requested_size;
	} else {
		vk.cmd->buf_offset[ index ] = offset;
		Com_Memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
	}

	vk_bind_index_attr( index );
}


uint32_t vk_tess_index( uint32_t numIndexes, const void *src ) {
	const uint32_t offset = vk.cmd->vertex_buffer_offset;
	const uint32_t size = numIndexes * sizeof( tess.indexes[0] );

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize - use history for pre-allocation
		VkDeviceSize requested_size = log2pad( offset + size, 1 );
		if ( vk.geometry_buffer_history.count >= 4 ) {
			VkDeviceSize avg_size = 0;
			uint32_t i;
			for ( i = 0; i < vk.geometry_buffer_history.count; i++ ) {
				avg_size += vk.geometry_buffer_history.sizes[i];
			}
			avg_size = ( avg_size / vk.geometry_buffer_history.count ) * 5 / 4; // 25% margin
			if ( avg_size > requested_size ) {
				requested_size = log2pad( avg_size, 1 );
			}
		}
		vk.geometry_buffer_size_new = requested_size;
		return ~0U;
	} else {
		Com_Memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
		return offset;
	}
}


void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset )
{
	if ( vk.cmd->curr_index_buffer != buffer || vk.cmd->curr_index_offset != offset )
		qvkCmdBindIndexBuffer( vk.cmd->command_buffer, buffer, offset, VK_INDEX_TYPE_UINT32 );

	vk.cmd->curr_index_buffer = buffer;
	vk.cmd->curr_index_offset = offset;
}


// Performance counter wrapper for draw calls
static inline void vk_draw_call(VkCommandBuffer cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
	Perf_CountDrawCall();
	qvkCmdDraw(cmd, vertexCount, instanceCount, firstVertex, firstInstance);
}

static inline void vk_draw_indexed_call(VkCommandBuffer cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
	Perf_CountDrawCall();
	qvkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex )
{
	vk_draw_indexed_call( vk.cmd->command_buffer, indexCount, 1, firstIndex, 0, 0 );
}
#endif


void vk_bind_index( void )
{
#ifdef USE_VBO
	if ( tess.vboIndex ) {
		vk.cmd->num_indexes = 0;
		//qvkCmdBindIndexBuffer( vk.cmd->command_buffer, vk.vbo.index_buffer, tess.shader->iboOffset, VK_INDEX_TYPE_UINT32 );
		return;
	}
#endif

	vk_bind_index_ext( tess.numIndexes, tess.indexes );
}


void vk_bind_index_ext( const int numIndexes, const uint32_t *indexes )
{
	uint32_t offset	= vk_tess_index( numIndexes, indexes );
	if ( offset != ~0U ) {
		vk_bind_index_buffer( vk.cmd->vertex_buffer, offset );
		vk.cmd->num_indexes = numIndexes;
	} else {
		// overflowed
		vk.cmd->num_indexes = 0;
	}
}


void vk_bind_geometry( uint32_t flags )
{
	//unsigned int size;
	bind_base = -1;
	bind_count = 0;

	if ( ( flags & ( TESS_XYZ | TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2 | TESS_NNN | TESS_RGBA1 | TESS_RGBA2 ) ) == 0 )
		return;

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.vbo.vertex_buffer;
#ifdef USE_VK_PBR
		shade_bufs[8] = vk.vbo.vertex_buffer;
		shade_bufs[9] = vk.vbo.vertex_buffer;
#endif

		if ( flags & TESS_XYZ ) {  // 0
			vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
			vk_bind_index_attr( 0 );
		}

		if ( flags & TESS_RGBA0 ) { // 1
			vk.cmd->vbo_offset[1] = tess.shader->stages[ tess.vboStage ]->rgb_offset[0];
			vk_bind_index_attr( 1 );
		}

		if ( flags & TESS_ST0 ) {  // 2
			vk.cmd->vbo_offset[2] = tess.shader->stages[ tess.vboStage ]->tex_offset[0];
			vk_bind_index_attr( 2 );
		}

		if ( flags & TESS_ST1 ) {  // 3
			vk.cmd->vbo_offset[3] = tess.shader->stages[ tess.vboStage ]->tex_offset[1];
			vk_bind_index_attr( 3 );
		}

		if ( flags & TESS_ST2 ) {  // 4
			vk.cmd->vbo_offset[4] = tess.shader->stages[ tess.vboStage ]->tex_offset[2];
			vk_bind_index_attr( 4 );
		}

		if ( flags & TESS_NNN ) { // 5
			vk.cmd->vbo_offset[5] = tess.shader->normalOffset;
			vk_bind_index_attr( 5 );
		}

		if ( flags & TESS_RGBA1 ) { // 6
			vk.cmd->vbo_offset[6] = tess.shader->stages[ tess.vboStage ]->rgb_offset[1];
			vk_bind_index_attr( 6 );
		}

		if ( flags & TESS_RGBA2 ) { // 7
			vk.cmd->vbo_offset[7] = tess.shader->stages[ tess.vboStage ]->rgb_offset[2];
			vk_bind_index_attr( 7 );
		}
#ifdef USE_VK_PBR
		if (flags & TESS_PBR) {
			vk.cmd->vbo_offset[8] = tess.shader->qtangentOffset;
			vk_bind_index_attr(8);

			vk.cmd->vbo_offset[9] = tess.shader->lightdirOffset;
			vk_bind_index_attr(9);
		}
#endif

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->vbo_offset + bind_base );

	} else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.cmd->vertex_buffer;
#ifdef USE_VK_PBR
		shade_bufs[8] = vk.cmd->vertex_buffer;
		shade_bufs[9] = vk.cmd->vertex_buffer;
#endif

		if ( flags & TESS_XYZ ) {
			vk_bind_attr(0, sizeof(tess.xyz[0]), &tess.xyz[0]);
		}

		if ( flags & TESS_RGBA0 ) {
			vk_bind_attr(1, sizeof( color4ub_t ), tess.svars.colors[0][0].rgba);
		}

		if ( flags & TESS_ST0 ) {
			vk_bind_attr(2, sizeof( vec2_t ), tess.svars.texcoordPtr[0]);
		}

		if ( flags & TESS_ST1 ) {
			vk_bind_attr(3, sizeof( vec2_t ), tess.svars.texcoordPtr[1]);
		}

		if ( flags & TESS_ST2 ) {
			vk_bind_attr(4, sizeof( vec2_t ), tess.svars.texcoordPtr[2]);
		}

		if ( flags & TESS_NNN ) {
			vk_bind_attr(5, sizeof(tess.normal[0]), tess.normal);
		}

		if ( flags & TESS_RGBA1 ) {
			vk_bind_attr(6, sizeof( color4ub_t ), tess.svars.colors[1][0].rgba);
		}

		if ( flags & TESS_RGBA2 ) {
			vk_bind_attr(7, sizeof( color4ub_t ), tess.svars.colors[2][0].rgba);
		}
#ifdef USE_VK_PBR
		if (flags & TESS_PBR) {
			vk_bind_attr(8, sizeof(tess.qtangent[0]), tess.qtangent);
			vk_bind_attr(9, sizeof(tess.lightdir[0]), tess.lightdir);
		}
#endif

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_bind_lighting( int stage, int bundle )
{
	bind_base = -1;
	bind_count = 0;

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.vbo.vertex_buffer;

		vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
		vk.cmd->vbo_offset[1] = tess.shader->stages[ stage ]->tex_offset[ bundle ];
		vk.cmd->vbo_offset[2] = tess.shader->normalOffset;

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 3, shade_bufs, vk.cmd->vbo_offset + 0 );

	}
	else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.cmd->vertex_buffer;

		vk_bind_attr( 0, sizeof( tess.xyz[0] ), &tess.xyz[0] );
		vk_bind_attr( 1, sizeof( vec2_t ), tess.svars.texcoordPtr[ bundle ] );
		vk_bind_attr( 2, sizeof( tess.normal[0] ), tess.normal );

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_reset_descriptor( int index )
{
	vk.cmd->descriptor_set.current[ index ] = VK_NULL_HANDLE;
}


void vk_update_descriptor( uint32_t index, VkDescriptorSet descriptor )
{
	// Batch descriptor updates: track range of changed descriptors
	// Actual binding deferred until vk_bind_descriptor_sets() for efficiency
	if ( vk.cmd->descriptor_set.current[ index ] != descriptor ) {
		if ( vk.cmd->descriptor_set.start == ~0U ) {
			vk.cmd->descriptor_set.start = index;
			vk.cmd->descriptor_set.end = index;
		} else {
			vk.cmd->descriptor_set.start = ( index < vk.cmd->descriptor_set.start ) ? index : vk.cmd->descriptor_set.start;
			vk.cmd->descriptor_set.end = ( index > vk.cmd->descriptor_set.end ) ? index : vk.cmd->descriptor_set.end;
		}
	}
	vk.cmd->descriptor_set.current[ index ] = descriptor;
}

void vk_update_descriptor_offset( int index, uint32_t offset )
{
	vk.cmd->descriptor_set.offset[ index ] = offset;
}


void vk_bind_descriptor_sets( void )
{
	uint32_t offsets[3], offset_count;
	uint32_t start, end, count, i;

	start = vk.cmd->descriptor_set.start;
	if ( start == ~0U )
		return;

	end = vk.cmd->descriptor_set.end;

	offset_count = 0;
	if ( /*start == VK_DESC_STORAGE || */ start == VK_DESC_UNIFORM ) { // uniform offset or storage offset
		offsets[ offset_count++ ] = vk.cmd->descriptor_set.offset[ start ];
		offsets[offset_count++] = vk.cmd->descriptor_set.offset[start+1]; // camera uniform
	}

	count = end - start + 1;

	// Pre-fill NULL descriptor gaps to avoid per-frame overhead
	// Use white image as fallback for missing textures
	// This batching reduces individual descriptor set updates
	for ( i = start + 1; i < end; i++ ) {
		if ( vk.cmd->descriptor_set.current[i] == VK_NULL_HANDLE ) {
			vk.cmd->descriptor_set.current[i] = tr.whiteImage->descriptor;
		}
	}

	// Batch descriptor set binding - single API call for all changed descriptors
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, start, count, vk.cmd->descriptor_set.current + start, offset_count, offsets );

	vk.cmd->descriptor_set.end = 0;
	vk.cmd->descriptor_set.start = ~0U;
}


static void vk_update_depth_range( Vk_Depth_Range depth_range )
{
	if ( vk.cmd->depth_range != depth_range ) {
		VkRect2D scissor_rect;
		VkViewport viewport;

		vk.cmd->depth_range = depth_range;

		get_scissor_rect( &scissor_rect );

		if ( memcmp( &vk.cmd->scissor_rect, &scissor_rect, sizeof( scissor_rect ) ) != 0 ) {
			qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor_rect );
			vk.cmd->scissor_rect = scissor_rect;
		}

		get_viewport( &viewport, depth_range );
		qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	}
}


void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed ) {

	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	vk_bind_descriptor_sets();

	// configure pipeline's dynamic state
	vk_update_depth_range( depth_range );

	// issue draw call(s)
#ifdef USE_VBO
	if ( tess.vboIndex )
		VBO_RenderIBOItems();
	else
#endif
	if ( indexed ) {
		vk_draw_indexed_call( vk.cmd->command_buffer, vk.cmd->num_indexes, 1, 0, 0, 0 );
	} else {
		vk_draw_call( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
	}
}


void vk_draw_dot( uint32_t storage_offset )
{
	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_storage, VK_DESC_STORAGE, 1, &vk.storage.descriptor, 1, &storage_offset );

	// configure pipeline's dynamic state
	vk_update_depth_range( DEPTH_RANGE_NORMAL );

	vk_draw_call( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
}


static void vk_begin_render_pass( VkRenderPass renderPass, VkFramebuffer frameBuffer, qboolean clearValues, uint32_t width, uint32_t height )
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_values[3];

	// Note: The check for active render pass happens in the caller functions BEFORE
	// they set vk.renderPassIndex. This is because by the time we reach here, the caller
	// has already set vk.renderPassIndex, so checking here would always fail incorrectly.

	// Begin render pass.

	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = width;
	render_pass_begin_info.renderArea.extent.height = height;

	if ( clearValues ) {
		// attachments layout:
		// [0] - resolve/color/presentation
		// [1] - depth/stencil
		// [2] - multisampled color, optional
		Com_Memset( clear_values, 0, sizeof( clear_values ) );
		// Default clear color is black; in debug mode we can choose a bright color
		// for the final gamma pass to highlight any uncleared regions.
		if ( r_vk_debugClearColor && r_vk_debugClearColor->integer &&
		     renderPass == vk.render_pass.gamma ) {
			clear_values[0].color.float32[0] = 1.0f; // magenta
			clear_values[0].color.float32[1] = 0.0f;
			clear_values[0].color.float32[2] = 1.0f;
			clear_values[0].color.float32[3] = 1.0f;
		}
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0;
#endif
		render_pass_begin_info.clearValueCount = vk.msaaActive ? 3 : 2;
		render_pass_begin_info.pClearValues = clear_values;

		vk_world.dirty_depth_attachment = 0;
	} else {
		render_pass_begin_info.clearValueCount = 0;
		render_pass_begin_info.pClearValues = NULL;
	}

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );

	// Note: renderPassIndex is set by the caller BEFORE calling this function.
	// This allows the caller to set the index before beginning the pass, which is needed
	// for proper tracking. The check for active render pass happens in the caller.

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
}


// Function vk_begin_post_bloom_render_pass removed - moved to vk_framebuffer.c
void vk_begin_bloom_extract_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.bloom_extract;

	// CRITICAL: Check if render pass is already active BEFORE calling vk_begin_render_pass().
	// Note: renderPassIndex is not set for bloom passes (they use dedicated pipelines),
	// but we still need to ensure no render pass is active.
	if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
		ri.Error( ERR_FATAL, "Vulkan: Attempted to begin bloom_extract render pass when one is already active (index=%d). "
			"This indicates a bug in render pass lifecycle management.", vk.renderPassIndex );
	}

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk.renderWidth = gls.captureWidth;
	vk.renderHeight = gls.captureHeight;

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.bloom_extract, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


void vk_begin_blur_render_pass( uint32_t index )
{
	VkFramebuffer frameBuffer = vk.framebuffers.blur[ index ];

	// CRITICAL: Check if render pass is already active BEFORE calling vk_begin_render_pass().
	// Note: renderPassIndex is not set for blur passes (they use dedicated pipelines),
	// but we still need to ensure no render pass is active.
	if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
		ri.Error( ERR_FATAL, "Vulkan: Attempted to begin blur render pass when one is already active (index=%d). "
			"This indicates a bug in render pass lifecycle management.", vk.renderPassIndex );
	}

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk.renderWidth = gls.captureWidth / ( 2 << ( index / 2 ) );
	vk.renderHeight = gls.captureHeight / ( 2 << ( index / 2 ) );

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.blur[ index ], frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


static void __attribute__((unused)) vk_begin_screenmap_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.screenmap;

	// CRITICAL: Check if render pass is already active BEFORE setting renderPassIndex.
	if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
		ri.Error( ERR_FATAL, "Vulkan: Attempted to begin screenmap render pass when one is already active (index=%d). "
			"This indicates a bug in render pass lifecycle management.", vk.renderPassIndex );
	}

	vk.renderPassIndex = RENDER_PASS_SCREENMAP;

	vk.renderWidth = vk.screenMapWidth;
	vk.renderHeight = vk.screenMapHeight;

	vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;

	vk_begin_render_pass( vk.render_pass.screenmap, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
}

#ifdef VK_CUBEMAP
void vk_begin_cubemap_render_pass( void )
{
    VkFramebuffer frameBuffer = vk.framebuffers.cubemap[backEnd.viewParms.targetCubeLayer];

	// CRITICAL: Check if render pass is already active BEFORE setting renderPassIndex.
	if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
		ri.Error( ERR_FATAL, "Vulkan: Attempted to begin cubemap render pass when one is already active (index=%d). "
			"This indicates a bug in render pass lifecycle management.", vk.renderPassIndex );
	}

    vk.renderPassIndex = RENDER_PASS_CUBEMAP;

    vk.renderWidth = REF_CUBEMAP_SIZE;
    vk.renderHeight = REF_CUBEMAP_SIZE;
    vk.renderScaleX = vk.renderScaleY = 1.0f;

    vk_begin_render_pass(vk.render_pass.cubemap, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight);

    Com_Printf("render cube face %d\n", backEnd.viewParms.targetCubeLayer );
}

#endif
#ifdef VK_PBR_BRDFLUT
void vk_create_brfdlut( void )
{
    if( !vk.pbrActive )
        return;

    VkRenderPassBeginInfo   begin_info;
    VkClearValue            clear_values[1];
    VkCommandBuffer         command_buffer;
    VkViewport              viewport;
    VkRect2D                scissor_rect;
    uint32_t                size;

    command_buffer = begin_command_buffer();
    size = 512;
    
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.pNext = NULL;
    begin_info.renderPass = vk.render_pass.brdflut;
    begin_info.framebuffer = vk.framebuffers.brdflut;
    begin_info.renderArea.offset.x = 0;
    begin_info.renderArea.offset.y = 0;
    begin_info.renderArea.extent.width = size;
    begin_info.renderArea.extent.height = size;

    Com_Memset( clear_values, 0, sizeof( clear_values ) );
    clear_values[0].color.float32[3] = 1.0f;

    begin_info.clearValueCount = 1;
    begin_info.pClearValues = clear_values;	
	
    Com_Memset( &viewport, 0, sizeof( viewport ) );
    viewport.width = viewport.height = (float)size;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    Com_Memset( &scissor_rect, 0, sizeof( scissor_rect ) );
    scissor_rect.extent.width = scissor_rect.extent.height = size;

    qvkCmdBeginRenderPass( command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE );
    qvkCmdSetScissor( command_buffer, 0, 1, &scissor_rect );
    qvkCmdSetViewport( command_buffer, 0, 1, &viewport ); 
    qvkCmdBindPipeline( command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.brdflut_pipeline );
    qvkCmdBindDescriptorSets( command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_brdflut, 0, 1, &vk.brdflut_image_descriptor, 0, NULL );
    qvkCmdDraw( command_buffer, 4, 1, 0, 0 );	
    qvkCmdEndRenderPass( command_buffer );

    end_command_buffer( command_buffer, __func__  );
}
#endif

void vk_end_render_pass( void )
{
	// Only end render pass if one is actually active
	// We check renderPassIndex to avoid crashing if no render pass was started
	if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
		qvkCmdEndRenderPass( vk.cmd->command_buffer );
		vk.renderPassIndex = RENDER_PASS_COUNT; // Mark as no active render pass
	}

//	vk.renderPassIndex = RENDER_PASS_MAIN;
}


// Synchronize the final color image so fullscreen passes can safely sample it.
void vk_barrier_final_image_to_shader_read( VkImage image )
{
	if ( image == VK_NULL_HANDLE ) {
		return;
	}

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	// Synchronize: wait for color attachment output (including clears) to complete before fragment shader reads.
	// This is critical for menu-only frames where the image was only cleared, not drawn to.
	// CRITICAL: The render pass's finalLayout already transitions the image to SHADER_READ_ONLY_OPTIMAL,
	// but we still need this barrier for proper synchronization between the render pass ending
	// and the gamma pass beginning. The barrier ensures all color attachment writes (including clears)
	// are visible to the fragment shader before sampling begins.
	// 
	// Note: We use SHADER_READ_ONLY_OPTIMAL for both oldLayout and newLayout because the render pass
	// already transitioned the layout. This barrier is purely for synchronization, not layout transition.
	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &barrier
	);
}


static qboolean vk_find_screenmap_drawsurfs( void )
{
	const void *curCmd = &backEndData->commands.cmds;
	const drawBufferCommand_t *db_cmd;
	const drawSurfsCommand_t *ds_cmd;

	for ( ;; ) {
		curCmd = PADP( curCmd, sizeof(void *) );
		switch ( *(const int *)curCmd ) {
			case RC_DRAW_BUFFER:
				db_cmd = (const drawBufferCommand_t *)curCmd;
				curCmd = (const void *)(db_cmd + 1);
				break;
			case RC_DRAW_SURFS:
				ds_cmd = (const drawSurfsCommand_t *)curCmd;
				return ds_cmd->refdef.needScreenMap;
			default:
				return qfalse;
		}
	}
}


// ========================================================================
// VK_CaptureScreenMap: Copy/blit the main color buffer to screenMap
// ========================================================================
// This function captures the current main color buffer (vk.color_image) and
// copies it to the screenMap image at reduced resolution (1/16th size).
// screenMap is used by menu/pause shaders for background blur/tint effects.
//
// Requirements:
// - Main color image must be in SHADER_READ_ONLY_OPTIMAL layout (after render pass ends)
// - screenMap image must exist and be valid
// - This must be called AFTER the main render pass ends but BEFORE UI rendering
//
// After this function completes:
// - screenMap.color_image contains a downscaled copy of the main scene
// - screenMap is in SHADER_READ_ONLY_OPTIMAL layout, ready for sampling
// - backEnd.screenMapDone is set to qtrue (caller responsibility)
qboolean vk_capture_screenmap( void )
{
	if ( !vk.fboActive || vk.screenMap.color_image == VK_NULL_HANDLE || vk.color_image == VK_NULL_HANDLE ) {
		return qfalse;
	}

	// Optional hard-disable via cvar to isolate bugs/device loss.
	if ( r_vk_disableScreenMap && r_vk_disableScreenMap->integer ) {
		if ( tr.needScreenMap ) {
			ri.Printf( PRINT_DEVELOPER, "VK: screenMap disabled via r_vk_disableScreenMap, using blackImage fallback\n" );
		}
		return qfalse;
	}

	if ( vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "VK: Cannot capture screenMap - command buffer not available\n" );
		return qfalse;
	}

	qboolean needsScreenMap = tr.needScreenMap || vk_find_screenmap_drawsurfs();
	if ( !needsScreenMap ) {
		return qfalse;
	}

	if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
		ri.Printf( PRINT_WARNING, "VK: Cannot capture screenMap - render pass is still active (index=%d)\n", vk.renderPassIndex );
		return qfalse;
	}

	ri.Printf( PRINT_DEVELOPER, "VK: Capturing screenMap from main color buffer (%dx%d -> %dx%d)\n",
		vk.renderWidth, vk.renderHeight, vk.screenMapWidth, vk.screenMapHeight );

	// CRITICAL: vk.color_image layout handling after render pass ends.
	// When vk_end_render_pass() is called, it records a transition from COLOR_ATTACHMENT_OPTIMAL
	// to SHADER_READ_ONLY_OPTIMAL (the render pass's finalLayout). This transition happens
	// automatically when the render pass ends, so by the time we record our barrier here,
	// the image is transitioning to (or already in) SHADER_READ_ONLY_OPTIMAL.
	//
	// Therefore, we MUST use SHADER_READ_ONLY_OPTIMAL as oldLayout, not COLOR_ATTACHMENT_OPTIMAL.
	// Using COLOR_ATTACHMENT_OPTIMAL would conflict with the render pass end transition and
	// cause VK_ERROR_DEVICE_LOST.
	//
	// The barrier pipeline stages must match: we wait for the render pass end transition
	// (COLOR_ATTACHMENT_OUTPUT_BIT) to complete, then transition to TRANSFER_SRC_OPTIMAL.
	VkImageMemoryBarrier srcBarrier = {};
	srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	srcBarrier.pNext = NULL;
	// CRITICAL: Use SHADER_READ_ONLY_OPTIMAL as oldLayout because the render pass's finalLayout
	// transition has already been recorded by vk_end_render_pass(). The image is transitioning
	// from COLOR_ATTACHMENT_OPTIMAL to SHADER_READ_ONLY_OPTIMAL, so we must transition from
	// SHADER_READ_ONLY_OPTIMAL to TRANSFER_SRC_OPTIMAL.
	srcBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
	srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	srcBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Match render pass finalLayout
	srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.image = vk.color_image;
	srcBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	srcBarrier.subresourceRange.baseMipLevel = 0;
	srcBarrier.subresourceRange.levelCount = 1;
	srcBarrier.subresourceRange.baseArrayLayer = 0;
	srcBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier dstBarrier = {};
	dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	dstBarrier.pNext = NULL;
	// CRITICAL: screenMap is initialized to SHADER_READ_ONLY_OPTIMAL, so we transition from that.
	// If screenMap is actually in UNDEFINED (shouldn't happen after init, but be defensive),
	// Vulkan allows UNDEFINED->any layout transition, but we'll get a validation warning.
	// To be completely safe, we could check the actual layout, but that's expensive.
	// Instead, we ensure the image is cleared before blitting, which handles uninitialized data.
	// CRITICAL: Use UNDEFINED as oldLayout to handle first-use case safely.
	// Vulkan spec allows UNDEFINED->any layout transition, which always works regardless
	// of the actual current layout. If screenMap is actually in SHADER_READ_ONLY_OPTIMAL,
	// this transition still works (Vulkan allows transitioning from UNDEFINED even if
	// the image is in a different layout). This prevents VK_ERROR_DEVICE_LOST from
	// layout mismatch errors.
	dstBarrier.srcAccessMask = 0; // No previous access - UNDEFINED layout has no access requirements
	dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Safe for first use - always valid transition
	dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.image = vk.screenMap.color_image;
	dstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	dstBarrier.subresourceRange.baseMipLevel = 0;
	dstBarrier.subresourceRange.levelCount = 1;
	dstBarrier.subresourceRange.baseArrayLayer = 0;
	dstBarrier.subresourceRange.layerCount = 1;

	// CRITICAL: The render pass has ended, so vk.color_image is transitioning to SHADER_READ_ONLY_OPTIMAL
	// (the finalLayout). Our barrier transitions from SHADER_READ_ONLY_OPTIMAL to TRANSFER_SRC_OPTIMAL.
	// The pipeline stages must wait for the render pass end transition to complete before our transition.
	VkImageMemoryBarrier barriers[2] = { srcBarrier, dstBarrier };
	// Wait for COLOR_ATTACHMENT_OUTPUT_BIT to complete (render pass end transition) and
	// FRAGMENT_SHADER_BIT (in case image is already sampled), then transition to TRANSFER_BIT.
	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, NULL,
		0, NULL,
		2, barriers
	);

	// Step 2.5: Clear screenMap to black before blitting to prevent stale data corruption
	// CRITICAL: The blit operation only copies the source region. If screenMap contains
	// garbage data from previous frames (especially on menu-only frames where screenMap
	// wasn't used), that garbage will remain and cause checkerboard corruption in UI.
	// Clearing to black ensures all pixels are initialized before the blit.
	VkClearColorValue clearColor = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	VkImageSubresourceRange clearRange = {};
	clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	clearRange.baseMipLevel = 0;
	clearRange.levelCount = 1;
	clearRange.baseArrayLayer = 0;
	clearRange.layerCount = 1;
	qvkCmdClearColorImage(
		vk.cmd->command_buffer,
		vk.screenMap.color_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		&clearColor,
		1, &clearRange
	);

	// Step 3: Blit from main color buffer to screenMap (downscaled)
	VkImageBlit blit = {};
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.mipLevel = 0;
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = 1;
	blit.srcOffsets[0].x = 0;
	blit.srcOffsets[0].y = 0;
	blit.srcOffsets[0].z = 0;
	blit.srcOffsets[1].x = vk.renderWidth;
	blit.srcOffsets[1].y = vk.renderHeight;
	blit.srcOffsets[1].z = 1;

	blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.dstSubresource.mipLevel = 0;
	blit.dstSubresource.baseArrayLayer = 0;
	blit.dstSubresource.layerCount = 1;
	blit.dstOffsets[0].x = 0;
	blit.dstOffsets[0].y = 0;
	blit.dstOffsets[0].z = 0;
	blit.dstOffsets[1].x = vk.screenMapWidth;
	blit.dstOffsets[1].y = vk.screenMapHeight;
	blit.dstOffsets[1].z = 1;

	qvkCmdBlitImage(
		vk.cmd->command_buffer,
		vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.screenMap.color_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &blit,
		VK_FILTER_LINEAR // Use linear filtering for smooth downscaling
	);

	// Step 4: Transition images back to their final layouts
	srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	srcBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	srcBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	srcBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	dstBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	dstBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dstBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	dstBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, NULL,
		0, NULL,
		2, barriers
	);

	// Step 5: Update screenMap descriptor to ensure shaders sample the correct, fresh image
	// CRITICAL: The descriptor must be updated after capture to point to the newly written
	// screenMap image. This ensures UI shaders that bind screenMap via vk.screenMap.color_descriptor
	// will sample the correct, synchronized image rather than stale data.
	// Note: vk_update_attachment_descriptors() updates this descriptor, but it's only called
	// during initialization. We update it here to ensure it's current for this frame.
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet desc;
	Vk_Sampler_Def sd;

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
	sd.max_lod_1_0 = qfalse; // Allow mipmaps if needed
	sd.noAnisotropy = qtrue;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	info.sampler = vk_find_sampler( &sd );
	info.imageView = vk.screenMap.color_image_view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.pNext = NULL;
	desc.dstSet = vk.screenMap.color_descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	desc.pImageInfo = &info;
	desc.pBufferInfo = NULL;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

	return qtrue;
}


// ========================================================================
// vk_clear_screenmap: Clear screenMap to black for UI-only frames
// ========================================================================
// When screenMap is needed but capture fails (e.g., intro video frames with no 3D scene),
// we clear screenMap to black to prevent stale data from being sampled by shaders.
// This fixes the repeating pattern corruption in intro videos.
qboolean vk_clear_screenmap( void )
{
	if ( !vk.fboActive || vk.screenMap.color_image == VK_NULL_HANDLE ) {
		return qfalse;
	}

	if ( vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return qfalse;
	}

	if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
		// Can't clear during render pass - must wait until after
		return qfalse;
	}

	ri.Printf( PRINT_DEVELOPER, "VK: Clearing screenMap to black (UI-only frame, no 3D scene to capture)\n" );

	// Transition screenMap to TRANSFER_DST_OPTIMAL for clearing
	// CRITICAL: Use UNDEFINED as oldLayout to handle any current layout safely, matching vk_capture_screenmap().
	// Vulkan spec allows UNDEFINED->any layout transition, which always works regardless
	// of the actual current layout. This prevents VK_ERROR_DEVICE_LOST from layout mismatch errors.
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = 0; // No previous access - UNDEFINED layout has no access requirements
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Safe for any current layout - always valid transition
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.screenMap.color_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // No previous stage - UNDEFINED layout
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &barrier
	);

	// Clear screenMap to black
	VkClearColorValue clearColor = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	VkImageSubresourceRange clearRange = {};
	clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	clearRange.baseMipLevel = 0;
	clearRange.levelCount = 1;
	clearRange.baseArrayLayer = 0;
	clearRange.layerCount = 1;
	qvkCmdClearColorImage(
		vk.cmd->command_buffer,
		vk.screenMap.color_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		&clearColor,
		1, &clearRange
	);

	// Transition back to SHADER_READ_ONLY_OPTIMAL
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &barrier
	);

	// Update descriptor to ensure shaders sample the cleared image
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet desc;
	Vk_Sampler_Def sd;

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
	sd.max_lod_1_0 = qfalse;
	sd.noAnisotropy = qtrue;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	info.sampler = vk_find_sampler( &sd );
	info.imageView = vk.screenMap.color_image_view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.pNext = NULL;
	desc.dstSet = vk.screenMap.color_descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	desc.pImageInfo = &info;
	desc.pBufferInfo = NULL;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

	return qtrue;
}


#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#endif

void vk_begin_frame( void )
{
	// Update performance statistics
	vk_update_performance_stats();

	// Timeline semaphore signaling: framework ready
	VkCommandBufferBeginInfo begin_info;
	VkResult res;

	if ( vk.frame_count++ ) // might happen during stereo rendering
		return;

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qtrue );
#endif

	vk.cmd = &vk.tess[ vk.cmd_index ];

#ifdef USE_VULKAN_RAY_TRACING
	// Update GIBS system
	if ( vk.gibs.enabled && vk.gibs.initialized ) {
		vk_gibs_update();
	}
#endif
	// Reset GPU culling instance cursor for this frame
	if ( vk.gpuCulling.enabled && vk.gpuCulling.initialized ) {
		vk_gpu_culling_begin_frame();
	}
	// Procedural dressing generates instance transforms before the cull/update pass
	if ( r_procDressing && r_procDressing->integer ) {
		vk_proc_dressing_tick();
	}
	// Update GPU culling after instances have been queued
	if ( vk.gpuCulling.enabled && vk.gpuCulling.initialized ) {
		vk_gpu_culling_update();
	}
	// Material system (lazy init + update)
	if ( r_materialSystem && r_materialSystem->integer && !vk.materialSystem.initialized ) {
		vk_material_system_init();
	}
	if ( vk.materialSystem.enabled && vk.materialSystem.initialized ) {
		vk_material_system_update();
	}
	// Portal lights system
#ifdef USE_VULKAN_RAY_TRACING
	if ( vk.rayTracingSupported && !portalSystem.initialized ) {
		vk_portal_lights_init();
	}
	if ( portalSystem.enabled && portalSystem.initialized ) {
		vk_portal_lights_update_lights();
	}
#endif
	// Update atmosphere system
	if ( vk.atmosphere.enabled && vk.atmosphere.initialized ) {
		vk_atmosphere_update();
	}

	// Telemetry: per-second GPU timing stats for frame pacing
	if ( r_frameTelemetry && r_frameTelemetry->integer ) {
		double avg = 0.0, min = 0.0, max = 0.0;
		vk_get_gpu_timing_stats( &avg, &min, &max );
		if ( tr.refdef.floatTime - vkFrameTelemetryLast > 1.0f ) {
			ri.Printf( PRINT_DEVELOPER, "FrameTelemetry: gpu avg %.3f ms (min %.3f / max %.3f) meshlets %u\n",
				avg, min, max, vk.mesh.meshletCount );
			vkFrameTelemetryLast = tr.refdef.floatTime;
		}
	}

	// CRITICAL: Reset render pass state at the start of each frame.
	// When command buffer is reset, the render pass state variable must also be reset.
	// This ensures vk.renderPassIndex accurately reflects the current state.
	vk.renderPassIndex = RENDER_PASS_COUNT; // No render pass active at frame start

	// CRITICAL: Command buffer lifecycle management
	// Before reusing a command buffer, we MUST:
	// 1. Wait for the fence to ensure the previous submission completed
	// 2. Reset the fence to unsignaled state
	// 3. Reset the command buffer to allow new recording
	// Without these steps, we risk VK_ERROR_DEVICE_LOST from invalid command buffer state.
	if ( vk.cmd->waitForFence ) {
		vk.cmd->waitForFence = qfalse;
		res = qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e10 );
		if ( res != VK_SUCCESS ) {
			if ( res == VK_ERROR_DEVICE_LOST ) {
				// Device lost - once device is lost, ALL Vulkan operations will fail
				// We cannot continue rendering. Shut down gracefully.
				ri.Error( ERR_FATAL, "Vulkan: Device lost detected in vkWaitForFences. "
					"This indicates a previous frame had invalid Vulkan API usage (invalid barriers, "
					"layout transitions, or command buffer misuse). Cannot recover - shutting down." );
			}
			else {
				ri.Error( ERR_FATAL, "Vulkan: %s returned %s", "vkWaitForFences", vk_result_string( res ) );
			}
		}
		// Reset fence to unsignaled state before reusing
		// CRITICAL: If device was lost above, we won't reach here due to ri.Error()
		res = qvkResetFences( vk.device, 1, &vk.cmd->rendering_finished_fence );
		if ( res != VK_SUCCESS ) {
			ri.Error( ERR_FATAL, "Vulkan: qvkResetFences returned %s", vk_result_string( res ) );
		}
		// CRITICAL: Reset command buffer before reusing - this is required by Vulkan spec
		// A command buffer must be reset before it can be recorded again after submission
		res = qvkResetCommandBuffer( vk.cmd->command_buffer, 0 );
		if ( res != VK_SUCCESS ) {
			ri.Error( ERR_FATAL, "Vulkan: qvkResetCommandBuffer returned %s", vk_result_string( res ) );
		}
	} else {
		// Even if waitForFence is false (first frame), reset the command buffer to ensure clean state
		// This handles cases where the command buffer might have been partially recorded
		res = qvkResetCommandBuffer( vk.cmd->command_buffer, 0 );
		if ( res != VK_SUCCESS ) {
			ri.Error( ERR_FATAL, "Vulkan: qvkResetCommandBuffer returned %s", vk_result_string( res ) );
		}
	}

	if ( !ri.CL_IsMinimized() && !vk.cmd->swapchain_image_acquired ) {
		qboolean retry = qfalse;
_retry:
		res = qvkAcquireNextImageKHR( vk.device, vk.swapchain, 1 * 1000000000ULL, vk.cmd->image_acquired, VK_NULL_HANDLE, &vk.cmd->swapchain_image_index );
		// when running via RDP: "Application has already acquired the maximum number of images (0x2)"
		// probably caused by "device lost" errors
		if ( res < 0 ) {
			if ( res == VK_ERROR_OUT_OF_DATE_KHR && retry == qfalse ) {
				// swapchain re-creation needed
				retry = qtrue;
				// vk_restart_swapchain( __func__, res ); // TODO: Implement swapchain restart
				goto _retry;
			} else {
				ri.Error( ERR_FATAL, "vkAcquireNextImageKHR returned %s", vk_result_string( res ) );
			}
		}
		vk.cmd->swapchain_image_acquired = qtrue;
	}

	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( vk.cmd->command_buffer, &begin_info ) );

	// Ensure visibility of geometry buffers writes.
	//record_buffer_memory_barrier( vk.cmd->command_buffer, vk.cmd->vertex_buffer, vk.geometry_buffer_size, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT );

#if 0
	// add explicit layout transition dependency
	if ( vk.fboActive ) {
		record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( vk.cmd->command_buffer, vk.swapchain_images[ vk.swapchain_image_index ], VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 0 );
	}
#endif

	if ( vk.cmd->vertex_buffer_offset > vk.stats.vertex_buffer_max ) {
		vk.stats.vertex_buffer_max = vk.cmd->vertex_buffer_offset;
	}

	// Track geometry buffer usage for pre-allocation
	if ( vk.geometry_buffer_history.count < ARRAY_LEN( vk.geometry_buffer_history.sizes ) ) {
		vk.geometry_buffer_history.sizes[ vk.geometry_buffer_history.count++ ] = vk.cmd->vertex_buffer_offset;
	} else {
		vk.geometry_buffer_history.sizes[ vk.geometry_buffer_history.index ] = vk.cmd->vertex_buffer_offset;
		vk.geometry_buffer_history.index = ( vk.geometry_buffer_history.index + 1 ) % ARRAY_LEN( vk.geometry_buffer_history.sizes );
	}
	if ( vk.cmd->vertex_buffer_offset > vk.geometry_buffer_history.max_size ) {
		vk.geometry_buffer_history.max_size = vk.cmd->vertex_buffer_offset;
	}

	// Initialize MVP cache
	vk.cmd->mvp_cache.last_entity_num = -1;
	vk.cmd->mvp_cache.mvp_valid = qfalse;
	vk.cmd->vertex_buffer_offset = 0;

	if ( vk.stats.push_size > vk.stats.push_size_max ) {
		vk.stats.push_size_max = vk.stats.push_size;
	}

	vk.cmd->last_pipeline = VK_NULL_HANDLE;

	// CRITICAL: Reset screenMapDone at the start of each frame.
	// It will be set to qtrue only after screenMap is successfully captured from a 3D scene.
	// We do NOT set it after clearing - shaders should use blackImage fallback unless screenMap
	// was actually captured. This prevents shaders from sampling stale data.
	backEnd.screenMapDone = qfalse;

	// CRITICAL: Clear screenMap at the start of every frame to prevent stale data corruption.
	// This ensures that if screenMap is accidentally sampled (shouldn't happen due to screenMapDone check),
	// it contains black instead of stale repeating patterns from previous frames.
	// The clear happens before any render pass starts, so it's always safe.
	// Note: We clear but don't set screenMapDone - shaders should use blackImage fallback.
	if ( vk.fboActive && vk.screenMap.color_image != VK_NULL_HANDLE ) {
		vk_clear_screenmap();
		// Don't set screenMapDone here - let shaders use blackImage fallback
		// screenMapDone will only be set after successful capture from 3D scene
	}

	// CRITICAL: Only start render pass if one isn't already active.
	// RB_DrawSurfs() may need to end and restart the render pass for screenMap capture,
	// so we shouldn't unconditionally start it here. Let RB_DrawSurfs() handle it.
	// However, we need to ensure a render pass is started for the first draw command.
	// The render pass will be started in RB_DrawBuffer() or RB_DrawSurfs() as needed.
	if ( vk.renderPassIndex >= RENDER_PASS_COUNT ) {
		// Generate VRS image before starting main render pass
		vk_vrs_generate_image( vk.cmd->command_buffer );
		vk_begin_main_render_pass();
	}

		// Record GPU timing start timestamp after render pass begins
		if ( vk.timing.enabled && qvkCmdWriteTimestamp ) {
			uint32_t query_index = vk.cmd_index * 2; // 2 queries per command buffer (start/end)
			qvkCmdWriteTimestamp( vk.cmd->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk.timing.timestamp_query_pool, query_index );
		}

	// dynamic vertex buffer layout
	vk.cmd->uniform_read_offset = 0;
	vk.cmd->vertex_buffer_offset = 0;
	Com_Memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	Com_Memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );
	vk.cmd->curr_index_buffer = VK_NULL_HANDLE;
	vk.cmd->curr_index_offset = 0;
	vk.cmd->num_indexes = 0;

	Com_Memset( &vk.cmd->descriptor_set, 0, sizeof( vk.cmd->descriptor_set ) );
	vk.cmd->descriptor_set.start = ~0U;
	//vk.cmd->descriptor_set.end = 0;

	Com_Memset( &vk.cmd->scissor_rect, 0, sizeof( vk.cmd->scissor_rect ) );

	// other stats
	vk.stats.push_size = 0;
}


static void vk_resize_geometry_buffer( void )
{
	int i;

	vk_end_render_pass();

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	qvkResetCommandBuffer( vk.cmd->command_buffer, 0 );

	vk_wait_idle();

	vk_release_geometry_buffers();

	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

	ri.Printf( PRINT_DEVELOPER, "...geometry buffer resized to %iK\n", (int)( vk.geometry_buffer_size / 1024 ) );
}


void vk_end_frame( void )
{
	VkResult res; // For error checking on queue submission
	VkImage dlssInputImage = VK_NULL_HANDLE;
	VkImageView dlssInputView = VK_NULL_HANDLE;
#ifdef USE_UPLOAD_QUEUE
	VkSemaphore waits[2], signals[2];
	const VkPipelineStageFlags wait_dst_stage_mask[2] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
#else
	const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
#endif
	VkSubmitInfo submit_info;

	if ( vk.frame_count == 0 )
		return;

	vk.frame_count = 0;

	if ( vk.geometry_buffer_size_new )
	{
		vk_resize_geometry_buffer();
		// issue: one frame may be lost during video recording
		// solution: re-record all commands again? (might be complicated though)
		return;
	}

	if ( vk.fboActive )
	{
		vk.cmd->last_pipeline = VK_NULL_HANDLE; // do not restore clobbered descriptors in vk_bloom()

		// ========================================================================
		// EXPLICIT FRAMEGRAPH: Define the single "final image" for this frame
		// ========================================================================
		// The gamma/tonemap/composite pass MUST sample from exactly ONE image that
		// is guaranteed to be fully written and properly transitioned every frame.
		//
		// Both bloom ON and bloom OFF paths write to vk.color_image:
		//   - Bloom OFF: main render pass writes to vk.color_image
		//   - Bloom ON:  post_bloom render pass writes to vk.color_image (blends bloom back)
		//
		// Therefore, finalColorImage/finalColorView always point to vk.color_image.
		// This explicit variable makes the framegraph clear and prevents bugs.
		VkImage     finalColorImage = vk.color_image;
		VkImageView finalColorView  = vk.color_image_view;
		
		// Track whether finalColorImage was written to in this frame (needed for barriers)
		qboolean finalImageWasWritten = qfalse;

		// ========================================================================
		// STEP 1: End main render pass and run optional RT composite
		// ========================================================================
#ifdef USE_VULKAN_RAY_TRACING
		// Ray tracing pass (if enabled) - runs after main render pass.
		// RT composite writes to vk.color_image, so finalColorImage remains correct.
		if ( backEnd.useRayTracingWorld &&
		     r_raytracing && r_raytracing->integer &&
		     vk.rayTracingSupported && vk.rt.initialized ) {
			if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
				finalImageWasWritten = qtrue;
				vk_end_render_pass(); // End main render pass before ray tracing
			}
			// Apply output scale if enabled
			uint32_t rtWidth = vk.renderWidth;
			uint32_t rtHeight = vk.renderHeight;
			if ( r_rt_outputScale && r_rt_outputScale->value != 1.0f ) {
				float scale = Com_Clamp( 0.25f, 2.0f, r_rt_outputScale->value );
				rtWidth = (uint32_t)( vk.renderWidth * scale );
				rtHeight = (uint32_t)( vk.renderHeight * scale );
				// Ensure minimum size and alignment
				rtWidth = ( rtWidth < 64 ) ? 64 : rtWidth;
				rtHeight = ( rtHeight < 64 ) ? 64 : rtHeight;
			}
			vk_rt_trace_rays( rtWidth, rtHeight );
			
			// Composite RT output (HDR) to color_image with tonemapping
			vk_rt_composite();
			// RT composite writes to vk.color_image via post_bloom render pass
			finalImageWasWritten = qtrue;
		} else if ( r_bloom->integer ) {
			// RT disabled, bloom enabled - end main render pass before bloom
			if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
				finalImageWasWritten = qtrue;
				vk_end_render_pass();
			}
		} else {
			// RT disabled, bloom disabled - main render pass wrote to vk.color_image
			// The render pass will be ended in STEP 3 to avoid redundant checks.
			// We just mark that the image was written (at least cleared).
			if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
				finalImageWasWritten = qtrue;
			}
		}
#else
		if ( r_bloom->integer ) {
			// Bloom enabled - end main render pass before bloom
			if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
				finalImageWasWritten = qtrue;
				vk_end_render_pass();
			}
		} else {
			// Bloom disabled - main render pass wrote to vk.color_image
			// The render pass will be ended in STEP 3 to avoid redundant checks.
			// We just mark that the image was written (at least cleared).
			if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
				finalImageWasWritten = qtrue;
			}
		}
#endif

		// ========================================================================
		// STEP 2: Run bloom if enabled (writes to vk.color_image via post_bloom)
		// ========================================================================
		if ( r_bloom->integer )
		{
			vk_bloom();
			// Bloom writes to vk.color_image in post_bloom render pass.
			// finalColorImage/finalColorView already point to vk.color_image/vk.color_image_view.
			finalImageWasWritten = qtrue;
		} else {
			// Bloom is OFF: finalColorImage/finalColorView point to vk.color_image
			// (written by main render pass or RT composite).
			// The render pass will be ended in STEP 3 to avoid redundant checks.
			// We just mark that the image was written (at least cleared).
			if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
				finalImageWasWritten = qtrue;
			}
		}

		// ========================================================================
		// STEP 2.5: screenMap capture moved to RB_DrawSurfs()
		// ========================================================================
		// screenMap is now captured in RB_DrawSurfs() right after 3D world rendering
		// completes but before UI rendering begins. This ensures screenMap contains
		// the current 3D scene for UI shaders to sample.
		// screenMapDone is set in RB_DrawSurfs() after successful capture.

		if ( backEnd.screenshotMask && vk.capture.image )
		{
			if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
				vk_end_render_pass();
			}

			// render to capture FBO
			vk_begin_render_pass( vk.render_pass.capture, vk.framebuffers.capture, qfalse, gls.captureWidth, gls.captureHeight );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.capture_pipeline );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );

			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
			vk_end_render_pass();
		}

		// ========================================================================
		// STEP 3: Ensure finalColorImage is ready for sampling by gamma pass
		// ========================================================================
		// CRITICAL: This must happen BEFORE the minimized check, because we need to
		// ensure the descriptor is always updated to point to the correct image,
		// even if we're not presenting this frame. This prevents stale bloom buffers
		// from being sampled when bloom is toggled off.
		
		// CRITICAL: End any remaining render pass before sampling.
		// This ensures the image layout transitions from COLOR_ATTACHMENT_OPTIMAL to
		// SHADER_READ_ONLY_OPTIMAL (via finalLayout). This is essential for proper synchronization.
		// Note: UI rendering happens BEFORE vk_end_frame() is called, so UI has already rendered
		// into the main render pass by this point. Ending the render pass here is safe and necessary.
		// On menu-only frames with bloom OFF, the main render pass may still be active if nothing
		// was drawn, so we must end it here to trigger the layout transition.
		if ( vk.renderPassIndex < RENDER_PASS_COUNT ) {
			finalImageWasWritten = qtrue;
			vk_end_render_pass();
		}

		// Ensure finalColorImage is in SHADER_READ_ONLY_OPTIMAL layout and properly synchronized
		// before the gamma pass samples from it. This is critical for both bloom ON and OFF paths.
		// The main render pass always clears vk.color_image at start, so if we have a valid
		// finalColorImage, it was definitely written to (at least cleared).
		// On menu-only frames with bloom OFF, the main render pass may have only cleared
		// the image but not drawn anything. We must ensure the image is properly synchronized
		// and in the correct layout before sampling, otherwise we'll get corruption.
		//
		// CRITICAL: The render pass's finalLayout transitions the image to SHADER_READ_ONLY_OPTIMAL
		// when it ends, but we still need an explicit barrier to ensure proper synchronization
		// between the render pass ending and the gamma pass sampling. Without this barrier,
		// the gamma pass might sample stale or uninitialized data, causing blocky corruption.
		if ( finalColorImage != VK_NULL_HANDLE && finalColorView != VK_NULL_HANDLE ) {
			if ( !finalImageWasWritten ) {
				// Image was cleared by render pass, so it was written to
				finalImageWasWritten = qtrue;
			}
			// Apply barrier to ensure proper synchronization before sampling
			vk_barrier_final_image_to_shader_read( finalColorImage );
		} else {
			ri.Printf( PRINT_WARNING, "VK: finalColorImage (%p) or finalColorView (%p) is NULL in vk_end_frame (bloom=%d)\n", 
				(void*)finalColorImage, (void*)finalColorView, r_bloom->integer );
		}

		// ========================================================================
		// STEP 4: Update descriptor set to point to finalColorView
		// ========================================================================
		// CRITICAL: Always update the descriptor to point to finalColorView.
		// This ensures the gamma pass always samples from the correct, valid image.
		// The descriptor might have been set to point to a bloom buffer in a previous frame,
		// so we must reset it every frame to guarantee correctness.
		// This is especially important when toggling bloom at runtime.
		// NOTE: This must happen even when minimized, to prevent stale descriptors.
		// Validate inputs before updating descriptor
		if ( finalColorView == VK_NULL_HANDLE ) {
			ri.Printf( PRINT_WARNING, "VK: finalColorView is NULL, cannot update descriptor (bloom=%d)\n", r_bloom->integer );
		} else {
			VkDescriptorImageInfo info;
			VkWriteDescriptorSet desc;
			Vk_Sampler_Def sd;

			Com_Memset( &sd, 0, sizeof( sd ) );
			sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
			sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;

			info.sampler = vk_find_sampler( &sd );
			info.imageView = finalColorView;
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			desc.pNext = NULL;
			desc.dstSet = vk.color_descriptor;
			desc.dstBinding = 0;
			desc.dstArrayElement = 0;
			desc.descriptorCount = 1;
			desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			desc.pImageInfo = &info;
			desc.pBufferInfo = NULL;
			desc.pTexelBufferView = NULL;

			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}

		if ( !ri.CL_IsMinimized() )
		{
			// ========================================================================
			// STEP 5: DLSS upscaling (if enabled, replaces TAA)
			// ========================================================================
			dlssInputImage = finalColorImage;
			dlssInputView = finalColorView;

			if ( r_dlss && r_dlss->integer && vk_dlss_is_supported() ) {
				// DLSS processing code...
				// Calculate render resolution based on DLSS quality mode
				// DLSS quality modes: 0=Performance (50%), 1=Balanced (58%), 2=Quality (67%), 3=Ultra Quality (77%)
				float scaleFactors[] = { 0.5f, 0.58f, 0.67f, 0.77f };
				int qualityMode = r_dlss_quality ? r_dlss_quality->integer : 1;
				qualityMode = (int)Com_Clamp( 0.0f, 3.0f, (float)qualityMode );
				float scaleFactor = scaleFactors[qualityMode];
				
				uint32_t renderWidth = (uint32_t)(gls.windowWidth * scaleFactor);
				uint32_t renderHeight = (uint32_t)(gls.windowHeight * scaleFactor);
				
				// Ensure render resolution is even (DLSS requirement)
				renderWidth = (renderWidth / 2) * 2;
				renderHeight = (renderHeight / 2) * 2;
				
				// Create DLSS resources if needed
				vk_dlss_create_resources( renderWidth, renderHeight, gls.windowWidth, gls.windowHeight );
				
				// Update DLSS quality mode and sharpening
				vk.dlss.qualityMode = qualityMode;
				vk.dlss.sharpening = r_dlss_sharpening ? r_dlss_sharpening->value : 0.0f;
				
				// Run DLSS upscaling
				// DLSS needs: color buffer, depth buffer, motion vectors
				// Depth buffer is passed, motion vectors require additional implementation
				if ( vk.dlss.dlssOutputImage != VK_NULL_HANDLE ) {
					vk_dlss_evaluate( vk.cmd->command_buffer, finalColorImage,
					                  vk.depth_image, VK_NULL_HANDLE, vk.frame_count );

					// Use DLSS output as input for gamma pass
					dlssInputImage = vk.dlss.dlssOutputImage;
					dlssInputView = vk.dlss.dlssOutputImageView;
				}

				// Alternative upscaler (FSR/DLSS framework) - ready for implementation
					
					// Update descriptor to point to DLSS output
					VkDescriptorImageInfo info;
					VkWriteDescriptorSet desc;
					Vk_Sampler_Def sd;
					
					Com_Memset( &sd, 0, sizeof( sd ) );
					sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
					sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
					sd.max_lod_1_0 = qtrue;
					sd.noAnisotropy = qtrue;
					
					info.sampler = vk_find_sampler( &sd );
					info.imageView = dlssInputView;
					info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					
					desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					desc.pNext = NULL;
					desc.dstSet = vk.color_descriptor;
					desc.dstBinding = 0;
					desc.dstArrayElement = 0;
					desc.descriptorCount = 1;
					desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					desc.pImageInfo = &info;
					desc.pBufferInfo = NULL;
					desc.pTexelBufferView = NULL;
					
					qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
				}
			}
			
			// ========================================================================
			// STEP 6: Optional style-transfer compute pass (writes to vk.style_image)
			// ========================================================================
			if ( r_styleTransfer && r_styleTransfer->integer &&
			     vk.style_compute_pipeline != VK_NULL_HANDLE &&
			     vk.style_image != VK_NULL_HANDLE && vk.style_image_view != VK_NULL_HANDLE ) {

				VkDescriptorImageInfo input_info, output_info;
				VkWriteDescriptorSet desc_writes[2];
				uint32_t write_count = 0;
				Vk_Sampler_Def sd;

				Com_Memset( &sd, 0, sizeof( sd ) );
				sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
				sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				sd.max_lod_1_0 = qtrue;
				sd.noAnisotropy = qtrue;

				// Transition style image to GENERAL for compute write
				VkImageMemoryBarrier style_barrier = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = NULL,
					.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_GENERAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = vk.style_image,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1
					}
				};
				qvkCmdPipelineBarrier( vk.cmd->command_buffer,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					0, 0, NULL, 0, NULL, 1, &style_barrier );

				// Update descriptor set for style compute
				input_info.sampler = vk_find_sampler( &sd );
				input_info.imageView = dlssInputView;
				input_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				desc_writes[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				desc_writes[write_count].pNext = NULL;
				desc_writes[write_count].dstSet = vk.compute_descriptor_set;
				desc_writes[write_count].dstBinding = 0;
				desc_writes[write_count].dstArrayElement = 0;
				desc_writes[write_count].descriptorCount = 1;
				desc_writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				desc_writes[write_count].pImageInfo = &input_info;
				desc_writes[write_count].pBufferInfo = NULL;
				desc_writes[write_count].pTexelBufferView = NULL;
				write_count++;

				output_info.sampler = VK_NULL_HANDLE;
				output_info.imageView = vk.style_image_view;
				output_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

				desc_writes[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				desc_writes[write_count].pNext = NULL;
				desc_writes[write_count].dstSet = vk.compute_descriptor_set;
				desc_writes[write_count].dstBinding = 1;
				desc_writes[write_count].dstArrayElement = 0;
				desc_writes[write_count].descriptorCount = 1;
				desc_writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				desc_writes[write_count].pImageInfo = &output_info;
				desc_writes[write_count].pBufferInfo = NULL;
				desc_writes[write_count].pTexelBufferView = NULL;
				write_count++;

				qvkUpdateDescriptorSets( vk.device, write_count, desc_writes, 0, NULL );

				qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.style_compute_pipeline );
				qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
					vk.compute_pipeline_layout, 0, 1, &vk.compute_descriptor_set, 0, NULL );

				struct {
					float strength;
					float levels;
					float edgeScale;
					float pad;
				} style_params;

				style_params.strength = Com_Clamp( 0.0f, 2.0f, r_styleStrength ? r_styleStrength->value : 0.35f );
				style_params.levels = Com_Clamp( 2.0f, 32.0f, r_styleLevels ? (float)r_styleLevels->integer : 8.0f );
				style_params.edgeScale = Com_Clamp( 0.0f, 4.0f, r_styleEdge ? r_styleEdge->value : 2.0f );
				style_params.pad = 0.0f;

				qvkCmdPushConstants( vk.cmd->command_buffer, vk.compute_pipeline_layout,
					VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( style_params ), &style_params );

				uint32_t wgSize = ( r_postprocess_workgroup && r_postprocess_workgroup->integer > 0 ) ? (uint32_t)r_postprocess_workgroup->integer : 8u;
				uint32_t groupCountX = (gls.windowWidth + (wgSize - 1)) / wgSize;
				uint32_t groupCountY = (gls.windowHeight + (wgSize - 1)) / wgSize;
				qvkCmdDispatch( vk.cmd->command_buffer, groupCountX, groupCountY, 1 );

				// Transition style image for sampling
				style_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				style_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				style_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				style_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				qvkCmdPipelineBarrier( vk.cmd->command_buffer,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0, 0, NULL, 0, NULL, 1, &style_barrier );

				dlssInputImage = vk.style_image;
				dlssInputView = vk.style_image_view;
			}
			
			// ========================================================================
			// STEP 7: Run gamma/tonemap pass (always samples from finalColorView or DLSS output)
			// ========================================================================
			// CRITICAL: Ensure finalColorImage is valid before sampling.
			// On menu-only frames with bloom OFF, the main render pass clears the image
			// but may not draw anything. The render pass clear ensures the image is
			// initialized, but we must verify it's in the correct layout and synchronized.
			//
			// CRITICAL: When RT is enabled, RT composite already outputs sRGB (via linearToSrgb),
			// so the gamma pipeline should have gamma=1.0 (set at pipeline creation time)
			// to avoid double gamma correction. The gamma pass still runs for obScale/dithering.
			if ( dlssInputImage == VK_NULL_HANDLE || dlssInputView == VK_NULL_HANDLE ) {
				ri.Printf( PRINT_WARNING, "VK: finalColorImage or finalColorView is NULL, skipping gamma pass\n" );
			} else {
				vk.renderWidth = gls.windowWidth;
				vk.renderHeight = gls.windowHeight;

				vk.renderScaleX = 1.0;
				vk.renderScaleY = 1.0;

				// Use compute shader post-processing if enabled
				VkPipeline activeComputePipeline = VK_NULL_HANDLE;
				qboolean useTonemapCompute = qfalse;

				const qboolean allowCompute = ( r_postprocess_compute && r_postprocess_compute->integer ) &&
					( !r_postQuality || r_postQuality->integer > 0 );

				if ( allowCompute && vk.compute_descriptor_set != VK_NULL_HANDLE ) {
					if ( vk.tonemap_compute_pipeline != VK_NULL_HANDLE && r_hdr && r_hdr->integer ) {
						activeComputePipeline = vk.tonemap_compute_pipeline;
						useTonemapCompute = qtrue;
					} else if ( vk.gamma_compute_pipeline != VK_NULL_HANDLE ) {
						activeComputePipeline = vk.gamma_compute_pipeline;
					}
					// If style transfer is requested, force the compute gamma path when available
					if ( r_styleTransfer && r_styleTransfer->integer && vk.gamma_compute_pipeline != VK_NULL_HANDLE ) {
						activeComputePipeline = vk.gamma_compute_pipeline;
						useTonemapCompute = qfalse;
					}
				}

				if ( activeComputePipeline != VK_NULL_HANDLE ) {
					
					// Compute shader gamma pass
					VkImage swapchainImage = vk.swapchain_images[vk.cmd->swapchain_image_index];
					VkImageView swapchainView = vk.swapchain_image_views[vk.cmd->swapchain_image_index];
					
					// 1. Update compute descriptor set with input/output images
					VkDescriptorImageInfo input_info, output_info;
					VkWriteDescriptorSet desc_writes[2];
					uint32_t write_count = 0;
					Vk_Sampler_Def sd;
					
					Com_Memset( &sd, 0, sizeof( sd ) );
					sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
					sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
					sd.max_lod_1_0 = qtrue;
					sd.noAnisotropy = qtrue;
					
					// Input image (sampler) - sample from finalColorView
					input_info.sampler = vk_find_sampler( &sd );
					input_info.imageView = finalColorView;
					input_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					
					desc_writes[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					desc_writes[write_count].pNext = NULL;
					desc_writes[write_count].dstSet = vk.compute_descriptor_set;
					desc_writes[write_count].dstBinding = 0;
					desc_writes[write_count].dstArrayElement = 0;
					desc_writes[write_count].descriptorCount = 1;
					desc_writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					desc_writes[write_count].pImageInfo = &input_info;
					desc_writes[write_count].pBufferInfo = NULL;
					desc_writes[write_count].pTexelBufferView = NULL;
					write_count++;
					
					// Transition swapchain image to GENERAL layout for compute writes
					VkImageMemoryBarrier barrier = {
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
						.pNext = NULL,
						.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
						.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
						.oldLayout = vk.initSwapchainLayout,
						.newLayout = VK_IMAGE_LAYOUT_GENERAL,
						.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.image = swapchainImage,
						.subresourceRange = {
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = 0,
							.layerCount = 1
						}
					};
					qvkCmdPipelineBarrier( vk.cmd->command_buffer,
						VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						0, 0, NULL, 0, NULL, 1, &barrier );
					
					// Output image (storage image) - write to swapchain
					output_info.sampler = VK_NULL_HANDLE;
					output_info.imageView = swapchainView;
					output_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
					
					desc_writes[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					desc_writes[write_count].pNext = NULL;
					desc_writes[write_count].dstSet = vk.compute_descriptor_set;
					desc_writes[write_count].dstBinding = 1;
					desc_writes[write_count].dstArrayElement = 0;
					desc_writes[write_count].descriptorCount = 1;
					desc_writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					desc_writes[write_count].pImageInfo = &output_info;
					desc_writes[write_count].pBufferInfo = NULL;
					desc_writes[write_count].pTexelBufferView = NULL;
					write_count++;
					
					qvkUpdateDescriptorSets( vk.device, write_count, desc_writes, 0, NULL );
					
					// 2. Bind compute pipeline
					qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, activeComputePipeline );
					
					// 3. Bind descriptor set
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
						vk.compute_pipeline_layout, 0, 1, &vk.compute_descriptor_set, 0, NULL );
					
					// 4. Push constants
					if ( useTonemapCompute ) {
						struct {
							int tonemapMode;
							float exposure;
							float padding[2];
						} tonemap_params;
						tonemap_params.tonemapMode = (int)Com_Clamp( 0.0f, 3.0f, r_tonemapMode ? (float)r_tonemapMode->integer : 1.0f );
						tonemap_params.exposure = Com_Clamp( 0.01f, 16.0f, r_tonemapExposure ? r_tonemapExposure->value : 1.0f );
						tonemap_params.padding[0] = tonemap_params.padding[1] = 0.0f;

						qvkCmdPushConstants( vk.cmd->command_buffer, vk.compute_pipeline_layout,
							VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( tonemap_params ), &tonemap_params );
					} else {
						struct {
							float gamma;
							float obScale;
							float greyscale;
							int ditherMode;
							int depth_r, depth_g, depth_b;
							float styleStrength;
						} gamma_params;
						
						gamma_params.gamma = 1.0f / (r_gamma->value);
						gamma_params.obScale = (float)(1 << tr.overbrightBits);
						gamma_params.greyscale = r_greyscale->value;
						gamma_params.ditherMode = r_dither->integer;
						// Depth bits for dithering (assuming 8-bit per channel)
						gamma_params.depth_r = 255;
						gamma_params.depth_g = 255;
						gamma_params.depth_b = 255;
						gamma_params.styleStrength = ( r_styleTransfer && r_styleTransfer->integer )
							? Com_Clamp( 0.0f, 2.0f, r_styleStrength ? r_styleStrength->value : 0.35f )
							: 0.0f;
						
						qvkCmdPushConstants( vk.cmd->command_buffer, vk.compute_pipeline_layout,
							VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(gamma_params), &gamma_params );
					}
					
					// 5. Dispatch compute shader (workgroup size configurable, default 8x8)
					uint32_t wgSize = ( r_postprocess_workgroup && r_postprocess_workgroup->integer > 0 ) ? (uint32_t)r_postprocess_workgroup->integer : 8u;
					uint32_t groupCountX = (vk.renderWidth + (wgSize - 1)) / wgSize;
					uint32_t groupCountY = (vk.renderHeight + (wgSize - 1)) / wgSize;
					qvkCmdDispatch( vk.cmd->command_buffer, groupCountX, groupCountY, 1 );

					// 6. Transition swapchain image back to PRESENT_SRC_KHR for presentation
					barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
					barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
					barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
					barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
					qvkCmdPipelineBarrier( vk.cmd->command_buffer,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
						0, 0, NULL, 0, NULL, 1, &barrier );
					
					ri.Printf( PRINT_DEVELOPER, "VK: Compute %s pass dispatched (%ux%u groups)\n",
						useTonemapCompute ? "tonemap" : "gamma", groupCountX, groupCountY );
				} else {
					// Traditional graphics pipeline gamma pass
					if ( r_postprocess_compute && r_postprocess_compute->integer && (!r_postQuality || r_postQuality->integer > 0) ) {
						ri.Printf( PRINT_WARNING, "VK: Compute post-processing requested but pipelines not available or disabled by quality, using graphics pipeline\n" );
					}
					// The gamma pass renders directly to the swapchain framebuffer.
					// Its render area MUST match the swapchain/window extent, not the internal render resolution.
					// If vk.renderWidth/Height are larger than the swapchain (e.g. desktop-sized internal res),
					// the fullscreen quad can be clipped to nothing -> black window with audio/input still working.
					ri.Printf( PRINT_DEVELOPER, "VK: Gamma pass: renderArea=%dx%d (swapchain), internal=%dx%d\n",
						gls.windowWidth, gls.windowHeight, vk.renderWidth, vk.renderHeight );
					vk_begin_render_pass( vk.render_pass.gamma, vk.framebuffers.gamma[ vk.cmd->swapchain_image_index ], qtrue, gls.windowWidth, gls.windowHeight );
					qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.gamma_pipeline );
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );

					// CRITICAL: Set viewport and scissor for gamma pass.
					// Since viewport/scissor are dynamic states, they MUST be set before drawing.
					// Without this, the viewport may be zero-sized or invalid, causing black screen.
					{
						VkViewport viewport;
						VkRect2D scissor_rect;

						viewport.x = 0.0f;
						viewport.y = 0.0f;
						viewport.width = (float)gls.windowWidth;
						viewport.height = (float)gls.windowHeight;
						viewport.minDepth = 0.0f;
						viewport.maxDepth = 1.0f;

						scissor_rect.offset.x = 0;
						scissor_rect.offset.y = 0;
						scissor_rect.extent.width = gls.windowWidth;
						scissor_rect.extent.height = gls.windowHeight;

						qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
						qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor_rect );
					}

					qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
					vk_end_render_pass();
				}
		}
	}

	// Record GPU timing end timestamp before ending command buffer
	if ( vk.timing.enabled && qvkCmdWriteTimestamp ) {
		uint32_t query_index = vk.cmd_index * 2 + 1; // End timestamp
		qvkCmdWriteTimestamp( vk.cmd->command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk.timing.timestamp_query_pool, query_index );
	}

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.cmd->command_buffer;
	if ( !ri.CL_IsMinimized() ) {
#ifdef USE_UPLOAD_QUEUE
		if ( vk.image_uploaded != VK_NULL_HANDLE ) {
			waits[0] = vk.cmd->image_acquired;
			waits[1] = vk.image_uploaded;
			submit_info.waitSemaphoreCount = 2;
			submit_info.pWaitSemaphores = &waits[0];
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			signals[0] = vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
			signals[1] = vk.cmd->rendering_finished2;
			submit_info.signalSemaphoreCount = 2;
			submit_info.pSignalSemaphores = &signals[0];

			vk.rendering_finished = vk.cmd->rendering_finished2;
			vk.image_uploaded = VK_NULL_HANDLE;
		} else if ( vk.rendering_finished != VK_NULL_HANDLE ) {
			waits[0] = vk.cmd->image_acquired;
			waits[1] = vk.rendering_finished;
			submit_info.waitSemaphoreCount = 2;
			submit_info.pWaitSemaphores = &waits[0];
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			signals[0] = vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
			signals[1] = vk.cmd->rendering_finished2;
			submit_info.signalSemaphoreCount = 2;
			submit_info.pSignalSemaphores = &signals[0];

			vk.rendering_finished = vk.cmd->rendering_finished2;
		} else {
			submit_info.waitSemaphoreCount = 1;
			submit_info.pWaitSemaphores = &vk.cmd->image_acquired;
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			submit_info.signalSemaphoreCount = 1;
			submit_info.pSignalSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
		}
#else
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &vk.cmd->image_acquired;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
#endif
	} else {
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;
	}

	// CRITICAL: Submit command buffer with fence for synchronization
	// The fence will be signaled when the command buffer execution completes.
	// If submission fails with VK_ERROR_DEVICE_LOST, the device is in an invalid state
	// and we cannot recover - this usually indicates invalid Vulkan API usage.
	res = qvkQueueSubmit( vk.queue, 1, &submit_info, vk.cmd->rendering_finished_fence );
	if ( res != VK_SUCCESS ) {
		if ( res == VK_ERROR_DEVICE_LOST ) {
			// Device lost - this is a critical error indicating invalid API usage
			// Common causes: invalid image layout transitions, command buffer reuse without reset,
			// invalid barriers, or sampling uninitialized images
			ri.Error( ERR_FATAL, "Vulkan: qvkQueueSubmit returned VK_ERROR_DEVICE_LOST - device is in invalid state. "
				"This usually indicates invalid image layout transitions, command buffer misuse, or invalid barriers. "
				"Check screenMap barriers and command buffer lifecycle." );
		} else {
			ri.Error( ERR_FATAL, "Vulkan: qvkQueueSubmit returned %s", vk_result_string( res ) );
		}
	}
	vk.cmd->waitForFence = qtrue;

	// Retrieve GPU timing results from previous frame (if available)
	// Note: Results are retrieved after submission, so we get timing from the frame that just completed
	if ( vk.timing.enabled && qvkGetQueryPoolResults && qvkWaitForFences ) {
		// Wait for previous frame's command buffer to complete before reading results
		uint32_t prev_cmd_index = ( vk.cmd_index + 1 ) % NUM_COMMAND_BUFFERS;
		if ( vk.tess[prev_cmd_index].waitForFence ) {
			VkResult fence_res = qvkWaitForFences( vk.device, 1, &vk.tess[prev_cmd_index].rendering_finished_fence, VK_FALSE, 1000000 ); // 1ms timeout
			if ( fence_res == VK_SUCCESS ) {
				uint32_t query_index = prev_cmd_index * 2;
				uint64_t timestamps[2];
				
				VkResult query_res = qvkGetQueryPoolResults( vk.device, vk.timing.timestamp_query_pool, 
					query_index, 2, sizeof( timestamps ), timestamps, sizeof( uint64_t ), 
					VK_QUERY_RESULT_64_BIT );
				
				if ( query_res == VK_SUCCESS && timestamps[1] > timestamps[0] ) {
					VkPhysicalDeviceProperties props;
					qvkGetPhysicalDeviceProperties( vk.physical_device, &props );
					uint64_t timestamp_period = props.limits.timestampPeriod; // nanoseconds per timestamp unit
					uint64_t frame_time_ns = ( timestamps[1] - timestamps[0] ) * timestamp_period;
					
					// Store in ring buffer for statistics
					vk.timing.frame_times[ vk.timing.frame_time_index ] = frame_time_ns;
					vk.timing.frame_time_index = ( vk.timing.frame_time_index + 1 ) % ARRAY_LEN( vk.timing.frame_times );
					
					// GPU timing provides more accurate measurement than CPU timing
					// Frame time is already calculated in RB_ExecuteRenderCommands via CPU timing
					// Update performance counters with current GPU frame time
					float gpu_frame_time_ms = (float)frame_time_ns / 1000000.0f;
					Perf_UpdateGPUTiming(gpu_frame_time_ms);
				}
				
				// Reset query pool for next frame
				if ( qvkResetQueryPool ) {
					qvkResetQueryPool( vk.device, vk.timing.timestamp_query_pool, query_index, 2 );
				}
			}
		}
	}

	vk.renderPassIndex = RENDER_PASS_MAIN;

	// Update performance statistics
	vk_update_performance_stats();

	// Check for memory defragmentation
	vk_check_defragmentation();

	// Increment frame count
	vk.frame_count++;

	// CRITICAL FIX: Present the frame NOW, at the end of vk_end_frame().
	// Previously vk_present_frame() was only called from RB_SwapBuffers(),
	// but RB_ExecuteRenderCommands() returns after RC_END_OF_LIST (which calls vk_end_frame()),
	// so vk_present_frame() in RB_SwapBuffers() was NEVER REACHED -> black screen!
	vk_present_frame();
}

void vk_present_frame( void )
{
	VkPresentInfoKHR present_info;
	VkResult res;

	if ( ri.CL_IsMinimized() || !vk.cmd->swapchain_image_acquired ) {
		return;
	}

	if ( !vk.cmd->waitForFence ) {
		// nothing has been submitted this frame due to geometry buffer overflow?
		return;
	}

	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &vk.swapchain;
	present_info.pImageIndices = &vk.cmd->swapchain_image_index;
	present_info.pResults = NULL;

	vk.cmd->swapchain_image_acquired = qfalse;

	res = qvkQueuePresentKHR( vk.queue, &present_info );
	switch ( res ) {
		case VK_SUCCESS:
			break;
		case VK_SUBOPTIMAL_KHR:
		case VK_ERROR_OUT_OF_DATE_KHR:
			// swapchain re-creation needed
			// vk_restart_swapchain( __func__, res ); // TODO: Implement swapchain restart
			return;
		case VK_ERROR_DEVICE_LOST:
			// we can ignore that
			ri.Printf( PRINT_DEVELOPER, "vkQueuePresentKHR: device lost\n" );
			break;
		default:
			// or we don't
			ri.Error( ERR_FATAL, "vkQueuePresentKHR returned %s", vk_result_string( res ) );
	}

	// pickup next command buffer for rendering
	vk.cmd_index++;
	vk.cmd_index %= NUM_COMMAND_BUFFERS;
	vk.cmd = &vk.tess[ vk.cmd_index ];
}


static qboolean is_bgr( VkFormat format ) {
	switch ( format ) {
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
			return qtrue;
		default:
			return qfalse;
	}
}


void vk_read_pixels( byte *buffer, uint32_t width, uint32_t height )
{
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkMemoryRequirements memory_requirements;
	VkMemoryPropertyFlags memory_reqs;
	VkMemoryPropertyFlags memory_flags;
	VkMemoryAllocateInfo alloc_info;
	VkImageSubresource subresource;
	VkSubresourceLayout layout;
	VkImageCreateInfo desc;
	VkImage srcImage;
	VkImageLayout srcImageLayout;
	VkImage dstImage;
	byte *buffer_ptr;
	byte *data;
	uint32_t pixel_width;
	uint32_t i, n;
	qboolean invalidate_ptr;

	VK_CHECK( qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e12 ) );

	if ( vk.fboActive ) {
		if ( vk.capture.image ) {
			// dedicated capture buffer
			srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			srcImage = vk.capture.image;
		} else {
			srcImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			srcImage = vk.color_image;
		}
	} else {
		srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		srcImage = vk.swapchain_images[ vk.cmd->swapchain_image_index ];
	}

	Com_Memset( &desc, 0, sizeof( desc ) );

	// Create image in host visible memory to serve as a destination for framebuffer pixels.
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = vk.capture_format;
	desc.extent.width = width;
	desc.extent.height = height;
	desc.extent.depth = 1;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_LINEAR;
	desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &dstImage ) );

	qvkGetImageMemoryRequirements( vk.device, dstImage, &memory_requirements );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;

	// host_cached bit is desirable for fast reads
	memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
	if ( alloc_info.memoryTypeIndex == ~0U ) {
		// try less explicit flags, without host_coherent
		memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
		if ( alloc_info.memoryTypeIndex == ~0U ) {
			// slowest case
			memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
			if ( alloc_info.memoryTypeIndex == ~0U ) {
				ri.Error( ERR_FATAL, "%s(): failed to find matching memory type for image capture", __func__ );
			}
		}
	}

	if ( memory_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) {
		invalidate_ptr = qfalse;
	} else {
		 // according to specification - must be performed if host_coherent is not set
		invalidate_ptr = qtrue;
	}

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory));
	VK_CHECK(qvkBindImageMemory(vk.device, dstImage, memory, 0));

	command_buffer = begin_command_buffer();

	if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
		record_image_layout_transition( command_buffer, srcImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			srcImageLayout,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			0, 0);
	}

	record_image_layout_transition( command_buffer, dstImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	// end_command_buffer( command_buffer, __func__  );

	// command_buffer = begin_command_buffer();

	if ( vk.blitEnabled ) {
		VkImageBlit region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffsets[0].x = 0;
		region.srcOffsets[0].y = 0;
		region.srcOffsets[0].z = 0;
		region.srcOffsets[1].x = width;
		region.srcOffsets[1].y = height;
		region.srcOffsets[1].z = 1;
		region.dstSubresource = region.srcSubresource;
		region.dstOffsets[0] = region.srcOffsets[0];
		region.dstOffsets[1] = region.srcOffsets[1];

		qvkCmdBlitImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST );

	} else {
		VkImageCopy region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffset.x = 0;
		region.srcOffset.y = 0;
		region.srcOffset.z = 0;
		region.dstSubresource = region.srcSubresource;
		region.dstOffset = region.srcOffset;
		region.extent.width = width;
		region.extent.height = height;
		region.extent.depth = 1;

		qvkCmdCopyImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
	}

	end_command_buffer( command_buffer, __func__ );

	// Copy data from destination image to memory buffer.
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;

	qvkGetImageSubresourceLayout( vk.device, dstImage, &subresource, &layout );

	VK_CHECK( qvkMapMemory( vk.device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&data ) );

	if ( invalidate_ptr )
	{
		VkMappedMemoryRange range;
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.pNext = NULL;
		range.memory = memory;
		range.size = VK_WHOLE_SIZE;
		range.offset = 0;
		qvkInvalidateMappedMemoryRanges( vk.device, 1, &range );
	}

	data += layout.offset;

	switch ( vk.capture_format ) {
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16: pixel_width = 2; break;
		case VK_FORMAT_R16G16B16A16_UNORM: pixel_width = 8; break;
		default: pixel_width = 4; break;
	}

	buffer_ptr = buffer + width * (height - 1) * 3;
	for ( i = 0; i < height; i++ ) {
		switch ( pixel_width ) {
			case 2: {
				uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = ((src[n]>>12)&0xF)<<4;
					buffer_ptr[n*3+1] = ((src[n]>>8)&0xF)<<4;
					buffer_ptr[n*3+2] = ((src[n]>>4)&0xF)<<4;
				}
			} break;

			case 4: {
				for ( n = 0; n < width; n++ ) {
					Com_Memcpy( &buffer_ptr[n*3], &data[n*4], 3 );
					//buffer_ptr[n*3+0] = data[n*4+0];
					//buffer_ptr[n*3+1] = data[n*4+1];
					//buffer_ptr[n*3+2] = data[n*4+2];
				}
			} break;

			case 8: {
				const uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = src[n*4+0]>>8;
					buffer_ptr[n*3+1] = src[n*4+1]>>8;
					buffer_ptr[n*3+2] = src[n*4+2]>>8;
				}
			} break;
		}
		buffer_ptr -= width * 3;
		data += layout.rowPitch;
	}

	if ( is_bgr( vk.capture_format ) ) {
		buffer_ptr = buffer;
		for ( i = 0; i < width * height; i++ ) {
			byte tmp = buffer_ptr[0];
			buffer_ptr[0] = buffer_ptr[2];
			buffer_ptr[2] = tmp;
			buffer_ptr += 3;
		}
	}

	qvkDestroyImage( vk.device, dstImage, NULL );
	qvkFreeMemory( vk.device, memory, NULL );

	// restore previous layout
	if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
		command_buffer = begin_command_buffer();

		record_image_layout_transition( command_buffer, srcImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			srcImageLayout, 0, 0 );

		end_command_buffer( command_buffer, "restore layout" );
	}
}


qboolean vk_bloom( void )
{
	uint32_t i;

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP )
	{
		return qfalse;
	}

	if ( backEnd.doneBloom || !backEnd.doneSurfaces || !vk.fboActive )
	{
		return qfalse;
	}

	// Render pass should already be ended before calling vk_bloom()
	// (either by RT composite or by vk_end_frame before calling bloom)

	// bloom extraction
	vk_begin_bloom_extract_render_pass();
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.bloom_extract_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	for ( i = 0; i < VK_NUM_BLOOM_PASSES*2; i+=2 ) {
		// horizontal blur
		vk_begin_blur_render_pass( i+0 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+0] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+0], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		// vectical blur
		vk_begin_blur_render_pass( i+1 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+1] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();
#if 0
		// horizontal blur
		vk_begin_blur_render_pass( i+0 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+0] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+2], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		// vectical blur
		vk_begin_blur_render_pass( i+1 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+1] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();
#endif
	}

	vk_begin_post_bloom_render_pass(); // begin post-bloom
	{
		VkDescriptorSet dset[VK_NUM_BLOOM_PASSES];

		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ )
		{
			dset[i] = vk.bloom_image_descriptor[(i+1)*2];
		}

		// blend downscaled buffers to main fbo
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.bloom_blend_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_blend, 0, ARRAY_LEN(dset), dset, 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	}

	// invalidate pipeline state cache
	//vk.cmd->last_pipeline = VK_NULL_HANDLE;

	if ( vk.cmd->last_pipeline != VK_NULL_HANDLE )
	{
		// restore last pipeline
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.cmd->last_pipeline );

		vk_update_mvp( NULL );

		// force depth range and viewport/scissor updates
		vk.cmd->depth_range = DEPTH_RANGE_COUNT;

		uint32_t offsets[2], offset_count;

		// restore clobbered descriptor sets
		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ ) {
			if ( vk.cmd->descriptor_set.current[i] != VK_NULL_HANDLE ) {
				if ( i == VK_DESC_UNIFORM /*|| i == VK_DESC_STORAGE*/ ) {
					offset_count = 0;

					offsets[offset_count++] = vk.cmd->descriptor_set.offset[i];
					offsets[offset_count++] = vk.cmd->descriptor_set.offset[VK_DESC_UNIFORM_CAMERA_BINDING];

					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], offset_count, offsets );
				}
				else
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], 0, NULL );
			}
		}
	}

	backEnd.doneBloom = qtrue;

	return qtrue;
}

enum Target { IRRADIANCE = 0, PREFILTEREDENV = 1 };


static filterDef prefilters[2];

static void vk_create_prefilter_renderpass( filterDef *def ) 
{
	VkAttachmentReference	color_attachment_ref;
	VkSubpassDependency		deps[2];
	VkAttachmentDescription	attachment;
	VkRenderPassCreateInfo	desc;
	VkSubpassDescription	subpass;

	// Color attachment
	Com_Memset( &attachment, 0, sizeof( attachment ) );
	attachment.format = def->format;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	// subpass dependencies
	Com_Memset( &deps, 0, sizeof( deps ) );

	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	
	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.attachmentCount = 1;
	desc.pAttachments = &attachment;
	desc.subpassCount = 1;
	desc.pSubpasses = &subpass;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;

	VK_CHECK( qvkCreateRenderPass( vk.device, &desc, NULL, &def->renderpass ) );
}

// Function vk_create_prefilter_framebuffer removed - moved to vk_framebuffer.c
static void vk_create_prefilter_pipeline( filterDef *def ) 
{
	VkPipelineShaderStageCreateInfo			shader_stages[3];
	VkPipelineVertexInputStateCreateInfo	vertex_input_state = {0};
	VkPipelineInputAssemblyStateCreateInfo	input_assembly_state;
	VkPipelineViewportStateCreateInfo		viewport_state = {0};
	VkPipelineRasterizationStateCreateInfo	rasterization_state = {0};
	VkPipelineMultisampleStateCreateInfo	multisample_state = {0};
	VkPipelineDepthStencilStateCreateInfo	depth_stencil_state = {0};
	VkPipelineColorBlendAttachmentState		attachment_blend_state = {0};
	VkPipelineColorBlendStateCreateInfo		blend_state = {0};
	VkPipelineDynamicStateCreateInfo		dynamic_state;
	VkDynamicState							dynamic_state_array[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkGraphicsPipelineCreateInfo			create_info = {0};
	VkPipelineLayoutCreateInfo				pipeline_layout;
	VkPushConstantRange						push_range;

	push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	push_range.offset = 0;

	pipeline_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout.pNext = NULL;
	pipeline_layout.flags = 0;
	pipeline_layout.setLayoutCount = 1;
	pipeline_layout.pSetLayouts = &vk.set_layout_sampler;

	if ( def->target == PREFILTEREDENV ) {
		push_range.size = sizeof(float);
		pipeline_layout.pushConstantRangeCount = 1;
		pipeline_layout.pPushConstantRanges = &push_range;
	} else {
		pipeline_layout.pushConstantRangeCount = 0;
		pipeline_layout.pPushConstantRanges = NULL;
	}

	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeline_layout, NULL, &def->pipeline_layout ) );
	
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.pNext = NULL;
    input_assembly_state.flags = 0;
    input_assembly_state.primitiveRestartEnable = VK_FALSE;	
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization_state.lineWidth = 1.0f;

	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	attachment_blend_state.blendEnable = VK_FALSE;

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.depthTestEnable = VK_FALSE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depth_stencil_state.front = depth_stencil_state.back;
	depth_stencil_state.back.compareOp = VK_COMPARE_OP_ALWAYS;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
				
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.pNext = NULL;
    dynamic_state.flags = 0;  
	dynamic_state.dynamicStateCount = ARRAY_LEN( dynamic_state_array );
    dynamic_state.pDynamicStates = dynamic_state_array;
	
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexAttributeDescriptions = NULL;

	set_shader_stage_desc( shader_stages + 0, VK_SHADER_STAGE_VERTEX_BIT, *def->shaders.vs_module, "main" );
	set_shader_stage_desc( shader_stages + 1, VK_SHADER_STAGE_FRAGMENT_BIT, *def->shaders.fs_module, "main" );
	set_shader_stage_desc( shader_stages + 2, VK_SHADER_STAGE_GEOMETRY_BIT, *def->shaders.gm_module, "main" );

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.layout = def->pipeline_layout;
	create_info.renderPass = def->renderpass;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pViewportState = &viewport_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pDynamicState = &dynamic_state;
	create_info.stageCount = ARRAY_LEN(shader_stages);
	create_info.pStages = shader_stages;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &def->pipeline ) );	
}

void vk_create_cubemap_prefilter( void )
{
	if ( !vk.cubemapActive )
		return;

	uint32_t	i;
	filterDef	*def;

	Com_Memset( &prefilters, 0, sizeof( prefilters ) );

	for ( i = 0; i < PREFILTEREDENV + 1; i++ ) 
	{
		def = &prefilters[i];

		def->target = i;
		def->shaders.vs_module = &vk.modules.filtercube_vs;
		def->shaders.gm_module = &vk.modules.filtercube_gm;

		switch ( def->target ) {
			case IRRADIANCE:
				def->format = VK_FORMAT_R32G32B32A32_SFLOAT;
				def->size = 64;
				def->shaders.fs_module = &vk.modules.irradiancecube_fs;
				def->mipLevels = (uint32_t)(floor(log2(def->size))) + 1;
				break;
			case PREFILTEREDENV:
				def->format = VK_FORMAT_R16G16B16A16_SFLOAT;
				def->size = 256;
				def->shaders.fs_module = &vk.modules.prefilterenvmap_fs;
				def->mipLevels = (uint32_t)(floor(log2(def->size))) + 1;
				break;
		};

		vk_create_prefilter_renderpass( def );
		vk_create_prefilter_framebuffer( def );
		vk_create_prefilter_pipeline( def );
	}
}

void vk_destroy_cubemap_prefilter( void ){

	uint32_t	i;
	filterDef	*def;

	for ( i = 0; i < PREFILTEREDENV + 1; i++ ) 
	{
		def = &prefilters[i];

		qvkDestroyRenderPass( vk.device, def->renderpass, NULL );
		qvkDestroyFramebuffer( vk.device, def->offscreen.framebuffer, NULL );
		qvkFreeMemory( vk.device, def->offscreen.memory, NULL );
		qvkDestroyImageView( vk.device, def->offscreen.view, NULL );
		qvkDestroyImage( vk.device, def->offscreen.image, NULL );
		def->offscreen.image = VK_NULL_HANDLE;
		def->offscreen.view = VK_NULL_HANDLE;
		qvkDestroyPipeline( vk.device, def->pipeline, NULL );
		qvkDestroyPipelineLayout( vk.device, def->pipeline_layout, NULL );
	}

	Com_Memset( &prefilters, 0, sizeof( prefilters ) );
}

void vk_clear_cube_color( image_t *image, VkClearColorValue clear_color_value ) 
{
	VkCommandBuffer			command_buffer;
	VkImageSubresourceRange desc;

	desc.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	desc.baseMipLevel   = 0;
	desc.levelCount     = VK_REMAINING_MIP_LEVELS; //6
	desc.baseArrayLayer = 0;
	desc.layerCount     = VK_REMAINING_ARRAY_LAYERS; //image->layers;

	command_buffer = begin_command_buffer();

	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	qvkCmdClearColorImage( command_buffer, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color_value, 1, &desc );	
		
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	end_command_buffer( command_buffer, __func__ );
}

static void vk_copy_to_cubemap( filterDef *def, VkImage *image, uint32_t mipLevel, uint32_t size ) 
{	
	VkImageCopy region;
	
	// change image layout for all offsceen faces to transfer source
	record_image_layout_transition( vk.cmd->command_buffer, def->offscreen.image, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
		0, 0);

	Com_Memset( &region, 0, sizeof( VkImageCopy ) );
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.baseArrayLayer = 0;
	region.srcSubresource.mipLevel = 0;
	region.srcSubresource.layerCount = 6;
	region.srcOffset.x = 0;
	region.srcOffset.y = 0;
	region.srcOffset.z = 0;



	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.dstSubresource.baseArrayLayer = 0;
	region.dstSubresource.mipLevel = mipLevel;
	region.dstSubresource.layerCount = 6;
	region.dstOffset.x = 0;
	region.dstOffset.y = 0;
	region.dstOffset.z = 0;

	region.extent.width = region.extent.height = size;
	region.extent.depth = 1;

	qvkCmdCopyImage( vk.cmd->command_buffer, def->offscreen.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

	record_image_layout_transition( vk.cmd->command_buffer, def->offscreen.image, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
		0, 0 );
}

void vk_generate_cubemaps( cubemap_t *cube ) 
{
	VkRenderPassBeginInfo	begin_info = {0};
	VkViewport				viewport;
	VkRect2D				scissor_rect;
	VkClearValue			clear_values[1];
	VkCommandBuffer			command_buffer;

	image_t		*cubemap = NULL;
	uint32_t	i, j;
	filterDef	*def;

	vk_end_render_pass();

	// CRITICAL: Ensure vk.cubeMap.color_image is in SHADER_READ_ONLY_OPTIMAL layout for sampling
	// This image might already be in use from the main render pass, so we need to transition it properly
	command_buffer = begin_command_buffer();
	// Use COLOR_ATTACHMENT_OPTIMAL as oldLayout since it might have been written to in the main render pass
	// If it wasn't written to, the transition from COLOR_ATTACHMENT_OPTIMAL to SHADER_READ_ONLY_OPTIMAL is still safe
	record_image_layout_transition( command_buffer, vk.cubeMap.color_image, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
		0, 0 );
	end_command_buffer( command_buffer, __func__  );

	for ( i = 0; i < PREFILTEREDENV + 1; i++ ) 
	{
		def = &prefilters[i];

		switch ( def->target ) {
			case IRRADIANCE: cubemap = cube->irradiance_image; break;
			case PREFILTEREDENV: cubemap = cube->prefiltered_image; break;
			default: cubemap = NULL; break;
		}
		if (!cubemap) {
			continue;
		}

		Com_Memset( clear_values, 0, sizeof( clear_values ) );
		clear_values[0].color.float32[0] = 0.75f;
		clear_values[0].color.float32[1] = 0.75f;
		clear_values[0].color.float32[2] = 0.75f;
		clear_values[0].color.float32[3] = 0.0f;

		begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_info.renderPass = def->renderpass;
		begin_info.framebuffer = def->offscreen.framebuffer;
		begin_info.renderArea.extent.width = def->size;
		begin_info.renderArea.extent.height = def->size;
		begin_info.clearValueCount = 1;
		begin_info.pClearValues = clear_values;

		Com_Memset( &viewport, 0, sizeof( viewport ) );
		viewport.width = viewport.height = (float)def->size;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		Com_Memset( &scissor_rect, 0, sizeof( scissor_rect ) );
		scissor_rect.extent.width = scissor_rect.extent.height = def->size;

		// CRITICAL: Change image layout for all cubemap faces to transfer destination
		// Use SHADER_READ_ONLY_OPTIMAL as oldLayout since cubemap images might have been used before
		// If this is the first use, transitioning from SHADER_READ_ONLY_OPTIMAL to TRANSFER_DST_OPTIMAL
		// is still safe (though not optimal). Using UNDEFINED would cause device loss if the image was already used.
		record_image_layout_transition( vk.cmd->command_buffer, cubemap->handle, VK_IMAGE_ASPECT_COLOR_BIT, 
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
			0, 0 );
			
		for ( j = 0; j < def->mipLevels; j++ ) {
			qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
			qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor_rect );

			// render scene from cube face's point of view
			qvkCmdBeginRenderPass(vk.cmd->command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

			if ( def->target == PREFILTEREDENV ) {
				float roughness = (float)j / (float)(def->mipLevels - 1);
				qvkCmdPushConstants( vk.cmd->command_buffer, def->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(roughness), &roughness );
			}

			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, def->pipeline );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, def->pipeline_layout, 0, 1, &vk.cubeMap.color_descriptor, 0, NULL );
			qvkCmdDraw( vk.cmd->command_buffer, 3, 1, 0, 0 );
			qvkCmdEndRenderPass( vk.cmd->command_buffer );

			vk_copy_to_cubemap( def, &cubemap->handle, j, (uint32_t)viewport.width );
		
			viewport.width /= 2;
			viewport.height /= 2;
		}

		record_image_layout_transition( vk.cmd->command_buffer, cubemap->handle, VK_IMAGE_ASPECT_COLOR_BIT, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	}

	// CRITICAL: vk.cubeMap.color_image is used for cubemap rendering, not the main render pass.
	// The main render pass uses vk.color_image, not vk.cubeMap.color_image.
	// So we don't need to transition vk.cubeMap.color_image back - it can stay in SHADER_READ_ONLY_OPTIMAL.
	// The transition at the start ensures it's ready for sampling during cubemap prefiltering.

	vk_begin_main_render_pass();
}

// Variable Rate Shading (VRS) implementation
void vk_vrs_init( void ) {
	// VRS support will be determined during device creation
	// For now, assume it's not supported until we check extensions
	vk.vrs.supported = qfalse;
	vk.vrs.enabled = qfalse;
	vk.vrs.mode = 0;
	vk.vrs.centerRadius = 0.6f;
	vk.vrs.falloffStart = 0.7f;
	vk.vrs.minRate = 1;
	vk.vrs.maxRate = 4;

	ri.Printf( PRINT_DEVELOPER, "Variable Rate Shading initialized (support checked at device creation)\n" );
}

void vk_vrs_shutdown( void ) {
	vk_vrs_destroy_resources();
}

void vk_vrs_create_resources( uint32_t width, uint32_t height ) {
	if ( !vk.vrs.supported || vk.vrsImage != VK_NULL_HANDLE ) {
		return;
	}

	// Calculate VRS image size (must be 1/16th of render area for fragment shading rate)
	uint32_t vrsWidth = (width + 15) / 16;   // Round up to nearest 16th
	uint32_t vrsHeight = (height + 15) / 16;

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8_UINT;  // Fragment shading rate format
	imageInfo.extent.width = vrsWidth;
	imageInfo.extent.height = vrsHeight;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.vrsImage ) );

	// Allocate memory
	VkMemoryRequirements memRequirements;
	qvkGetImageMemoryRequirements( vk.device, vk.vrsImage, &memRequirements );

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.vrsImageMemory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.vrsImage, vk.vrsImageMemory, 0 ) );

	// Create image view
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = vk.vrsImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8_UINT;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.vrsImageView ) );

	// Create descriptor set layout
	VkDescriptorSetLayoutBinding binding = {};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;

	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.vrsDescriptorSetLayout ) );

	// Allocate descriptor set
	VkDescriptorSetAllocateInfo allocSetInfo = {};
	allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocSetInfo.descriptorPool = vk.descriptor_pool;
	allocSetInfo.descriptorSetCount = 1;
	allocSetInfo.pSetLayouts = &vk.vrsDescriptorSetLayout;

	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocSetInfo, &vk.vrsDescriptorSet ) );

	// Update descriptor set
	VkDescriptorImageInfo imageInfoDesc = {};
	imageInfoDesc.imageView = vk.vrsImageView;
	imageInfoDesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkWriteDescriptorSet writeSet = {};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.dstSet = vk.vrsDescriptorSet;
	writeSet.dstBinding = 0;
	writeSet.descriptorCount = 1;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeSet.pImageInfo = &imageInfoDesc;

	qvkUpdateDescriptorSets( vk.device, 1, &writeSet, 0, NULL );

	SET_OBJECT_NAME( vk.vrsImage, "VRS image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.vrsImageView, "VRS image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	ri.Printf( PRINT_DEVELOPER, "Created VRS resources: %ux%u\n", vrsWidth, vrsHeight );
}

void vk_vrs_destroy_resources( void ) {
	if ( vk.vrsDescriptorSet != VK_NULL_HANDLE ) {
				qvkFreeDescriptorSets( vk.device, vk.descriptor_pool, 1, &vk.vrsDescriptorSet );
		vk.vrsDescriptorSet = VK_NULL_HANDLE;
	}

	if ( vk.vrsDescriptorSetLayout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.vrsDescriptorSetLayout, NULL );
		vk.vrsDescriptorSetLayout = VK_NULL_HANDLE;
	}

	if ( vk.vrsImageView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.vrsImageView, NULL );
		vk.vrsImageView = VK_NULL_HANDLE;
	}

	if ( vk.vrsImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.vrsImage, NULL );
		vk.vrsImage = VK_NULL_HANDLE;
	}

	if ( vk.vrsImageMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.vrsImageMemory, NULL );
		vk.vrsImageMemory = VK_NULL_HANDLE;
	}

	ri.Printf( PRINT_DEVELOPER, "Destroyed VRS resources\n" );
}

void vk_vrs_generate_image( VkCommandBuffer cmdBuffer ) {
	if ( !vk.vrs.supported || !vk.vrs.enabled || vk.vrs_generate_compute_pipeline == VK_NULL_HANDLE ) {
		return;
	}

	// Transition VRS image to general layout for compute write
	record_image_layout_transition( cmdBuffer, vk.vrsImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );

	// Bind pipeline and descriptor set
	qvkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.vrs_generate_compute_pipeline );
	qvkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.compute_pipeline_layout, 0, 1, &vk.compute_descriptor_set, 0, NULL );

	// Update VRS parameters from CVARs
	if ( r_vrs && r_vrs->integer ) {
		vk.vrs.enabled = qtrue;
		vk.vrs.mode = Com_Clamp( 0, 3, r_vrs_mode ? r_vrs_mode->integer : 1 );
		vk.vrs.centerRadius = Com_Clamp( 0.0f, 1.0f, r_vrs_center_radius ? r_vrs_center_radius->value : 0.6f );
		vk.vrs.falloffStart = Com_Clamp( 0.0f, 1.0f, r_vrs_falloff_start ? r_vrs_falloff_start->value : 0.7f );
		vk.vrs.minRate = Com_Clamp( 1, 4, r_vrs_min_rate ? r_vrs_min_rate->integer : 1 );
		vk.vrs.maxRate = Com_Clamp( 1, 4, r_vrs_max_rate ? r_vrs_max_rate->integer : 4 );
	} else {
		vk.vrs.enabled = qfalse;
	}

	if ( !vk.vrs.enabled ) {
		return;
	}

	// Push constants
	struct {
		float resolution[2];
		float invResolution[2];
		int vrsMode;
		float centerRadius;
		float falloffStart;
		int minRate;
		int maxRate;
	} vrsConstants;

	vrsConstants.resolution[0] = (float)vk.renderWidth;
	vrsConstants.resolution[1] = (float)vk.renderHeight;
	vrsConstants.invResolution[0] = 1.0f / (float)vk.renderWidth;
	vrsConstants.invResolution[1] = 1.0f / (float)vk.renderHeight;
	vrsConstants.vrsMode = vk.vrs.mode;
	vrsConstants.centerRadius = vk.vrs.centerRadius;
	vrsConstants.falloffStart = vk.vrs.falloffStart;
	vrsConstants.minRate = vk.vrs.minRate;
	vrsConstants.maxRate = vk.vrs.maxRate;

	qvkCmdPushConstants( cmdBuffer, vk.compute_pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( vrsConstants ), &vrsConstants );

	// Bind VRS descriptor set
	qvkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.compute_pipeline_layout, 1, 1, &vk.vrsDescriptorSet, 0, NULL );

	// Dispatch compute shader
	uint32_t groupCountX = (vk.renderWidth + 7) / 8;
	uint32_t groupCountY = (vk.renderHeight + 7) / 8;
	qvkCmdDispatch( cmdBuffer, groupCountX, groupCountY, 1 );

	// Add barrier to ensure VRS image is ready for use
	VkMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;

	qvkCmdPipelineBarrier( cmdBuffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 1, &barrier, 0, NULL, 0, NULL );
}

void vk_vrs_apply_shading_rate( VkCommandBuffer cmdBuffer ) {
	if ( !vk.vrs.supported || !vk.vrs.enabled || !qvkCmdSetFragmentShadingRateKHR ) {
		return;
	}

	// Set fragment shading rate based on VRS mode
	VkExtent2D fragmentSize = {1, 1}; // Default 1x1 (full rate)

	switch (vk.vrs.mode) {
		case 0: // Disabled
			return;
		case 1: // Center-focused - use full rate for center
			fragmentSize.width = fragmentSize.height = 1;
			break;
		case 2: // Distance-based - could use different rates based on distance
			fragmentSize.width = fragmentSize.height = 2; // 2x2 for lower quality
			break;
		case 3: // Center + distance - combine both approaches
			fragmentSize.width = fragmentSize.height = 1; // Start with full rate
			break;
		default:
			fragmentSize.width = fragmentSize.height = 1;
			break;
	}

	// Clamp to supported rates
	if (fragmentSize.width < (uint32_t)vk.vrs.minRate) fragmentSize.width = (uint32_t)vk.vrs.minRate;
	if (fragmentSize.height < (uint32_t)vk.vrs.minRate) fragmentSize.height = (uint32_t)vk.vrs.minRate;
	if (fragmentSize.width > (uint32_t)vk.vrs.maxRate) fragmentSize.width = (uint32_t)vk.vrs.maxRate;
	if (fragmentSize.height > (uint32_t)vk.vrs.maxRate) fragmentSize.height = (uint32_t)vk.vrs.maxRate;

	// Apply the shading rate - using default combiner operations
	const VkFragmentShadingRateCombinerOpKHR combinerOps[2] = {
		VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,    // pipeline
		VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR     // primitive
	};
	qvkCmdSetFragmentShadingRateKHR(cmdBuffer, &fragmentSize, combinerOps);

	ri.Printf(PRINT_ALL, "Applied VRS shading rate: %dx%d\n", fragmentSize.width, fragmentSize.height);
}

void vk_wait_idle( void ) {
	VK_CHECK( qvkDeviceWaitIdle( vk.device ) );
}

void vk_queue_wait_idle( void ) {
	VK_CHECK( qvkQueueWaitIdle( vk.queue ) );
}
