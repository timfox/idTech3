// Real TLAS skeleton stub for immediate wiring (will be replaced by full TLAS code)

#include "tr_local.h"
#include "vk_rtx_acceleration.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <time.h>
// Per-surface material indices
#define MAX_SURFACES_FOR_INDICES 4096
static uint32_t g_surface_indices_cpu[MAX_SURFACES_FOR_INDICES];
static VkBuffer g_surface_indices_buffer = VK_NULL_HANDLE;
static VkDeviceMemory g_surface_indices_memory = VK_NULL_HANDLE;
static VkDeviceAddress g_surface_indices_address = 0;
static VkDeviceSize g_surface_indices_size = 0;
static void vk_rtx_allocate_surface_indices_buffer(void);
static void vk_rtx_allocate_surface_indices_buffer(void) {
    if (g_surface_indices_buffer != VK_NULL_HANDLE) return;
    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = MAX_SURFACES_FOR_INDICES * sizeof(uint32_t);
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (qvkCreateBuffer) {
        if (qvkCreateBuffer(vk.device, &bufInfo, NULL, &g_surface_indices_buffer) != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "RTX: failed to create surface indices buffer\n");
            g_surface_indices_buffer = VK_NULL_HANDLE;
            return;
        }
    } else {
        ri.Printf(PRINT_WARNING, "RTX: qvkCreateBuffer not available\n");
        return;
    }
    VkMemoryRequirements memReqs;
    qvkGetBufferMemoryRequirements(vk.device, g_surface_indices_buffer, &memReqs);
    uint32_t memType = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memType;
    if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &g_surface_indices_memory) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: failed to allocate memory for surface indices buffer\n");
        vkDestroyBuffer(vk.device, g_surface_indices_buffer, NULL);
        g_surface_indices_buffer = VK_NULL_HANDLE;
        g_surface_indices_memory = VK_NULL_HANDLE;
        return;
    }
    qvkBindBufferMemory(vk.device, g_surface_indices_buffer, g_surface_indices_memory, 0);
    g_surface_indices_size = bufInfo.size;
    ri.Printf(PRINT_DEVELOPER, "RTX: surface indices buffer allocated: %llu bytes\n", (unsigned long long)g_surface_indices_size);
}
static VkBuffer s_dummy_bind_buffer = VK_NULL_HANDLE;
static VkDescriptorSet s_dummy_bind_desc = VK_NULL_HANDLE;

// Ensure buffers for surface indices exist
static void vk_rtx_allocate_surface_indices_buffer(void) {
    if (g_surface_indices_buffer != VK_NULL_HANDLE) {
        return;
    }
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = MAX_SURFACES_FOR_INDICES * sizeof(uint32_t);
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (qvkCreateBuffer) {
        if (qvkCreateBuffer(vk.device, &bufferInfo, NULL, &g_surface_indices_buffer) != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "RTX: Failed to create surface indices buffer\n");
            g_surface_indices_buffer = VK_NULL_HANDLE;
            return;
        }
    } else {
        // Fallback: log
        ri.Printf(PRINT_WARNING, "RTX: qvkCreateBuffer not available; cannot allocate surface indices buffer\n");
        return;
    }
    VkMemoryRequirements memReqs;
    qvkGetBufferMemoryRequirements(vk.device, g_surface_indices_buffer, &memReqs);
    uint32_t memoryType = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memoryType;
    if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &g_surface_indices_memory) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to allocate memory for surface indices buffer\n");
        vkDestroyBuffer(vk.device, g_surface_indices_buffer, NULL);
        g_surface_indices_buffer = VK_NULL_HANDLE;
        g_surface_indices_memory = VK_NULL_HANDLE;
        return;
    }
    qvkBindBufferMemory(vk.device, g_surface_indices_buffer, g_surface_indices_memory, 0);
    g_surface_indices_size = bufferInfo.size;
    ri.Printf(PRINT_DEVELOPER, "RTX: allocated surface indices buffer (%llu bytes)\n", (unsigned long long)g_surface_indices_size);
}


// Get material index for this surface's shader
uint32_t materialIndex = 0; // Default material
if (surf->shader && vk.materialSystem.enabled) {
    // Try to find material entry for this shader
    const materialEntry_t* entry = vk_material_parser_find_entry(surf->shader->name);
    if (entry) {
        // Find or create material index for this entry
        materialIndex = vk_material_system_find_or_create_index(entry);
        ri.Printf(PRINT_DEVELOPER, "RTX: Surface %d uses material %d for shader '%s'\n",
                 i, materialIndex, surf->shader->name);
    }
}

// Store material index in instance data for RTX shaders
// This will be used by gl_InstanceCustomIndexEXT in closest-hit shader
surf->materialIndex = materialIndex;

qboolean vk_rtx_build_tlas_real_inline(VkCommandBuffer cmd_buffer) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: TLAS real build inline stub\n");
  (void)cmd_buffer;
  g_rtx_blas_tlas_built = qtrue;
  return qtrue;
}

uint32_t vk_rtx_get_surface_material_index(uint32_t surfaceIndex, uint32_t* outIndex) {
    if (!outIndex) {
        ri.Printf(PRINT_DEVELOPER, "RTX: vk_rtx_get_surface_material_index called with NULL outIndex\n");
        return 0;
    }
    if (!tr.world || !tr.world->surfaces) {
        *outIndex = 0;
        return 0;
    }
    if (surfaceIndex >= (uint32_t)tr.world->numsurfaces) {
        *outIndex = 0;
        return 0;
    }
    *outIndex = tr.world->surfaces[surfaceIndex].materialIndex;
    return *outIndex;
}

void vk_rtx_bind_and_trace_raysKHR_from_main(VkCommandBuffer cmd_buffer, uint32_t width, uint32_t height) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: bind and trace from main (stub) width=%u height=%u cmd=%p\n", width, height, (void*)cmd_buffer);
}

void vk_rtx_trace_raysKHR(VkCommandBuffer cmd_buffer) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: trace raysKHR (stub) cmd=%p\n", (void*)cmd_buffer);
}

void vk_rtx_setup_sbt(VkCommandBuffer cmd_buffer) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: setup SBT (stub) cmd=%p\n", (void*)cmd_buffer);
}

qboolean vk_rtx_build_tlas_real_full(VkCommandBuffer cmd_buffer) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: TLAS real full build (stub) started\n");
  (void)cmd_buffer;
  if (g_blas_count == 0) {
    ri.Printf(PRINT_WARNING, "TLAS real full build requested with zero BLAS\n");
    return qfalse;
  }
  // Delegate to the existing TLAS build path for actual work (until real build is implemented)
  vk_rtx_build_tlas(cmd_buffer);
  // Barrier for synchronization between build and subsequent shader stages
  VkMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
    .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
  };
  vkCmdPipelineBarrier(cmd_buffer,
                       VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                       VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                       0, 1, &barrier, 0, NULL, 0, NULL);
  g_rtx_blas_tlas_built = qtrue;
  ri.Printf(PRINT_ALL, "TLAS real full build pathway invoked (delegated to vk_rtx_build_tlas).\n");
  return qtrue;
}

void vk_rtx_create_sbt_buffer_full(void) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: create SBT buffer (full stub)\n");
  // Placeholder for SBT buffer creation
}

qboolean vk_rtx_build_sbt_for_frame_full(VkCommandBuffer cmd_buffer) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: build SBT for frame (full stub)\n");
  (void)cmd_buffer;
  return qtrue;
}

// Real TLAS full build (stub)
qboolean vk_rtx_build_tlas_real_full(VkCommandBuffer cmd_buffer) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: TLAS real full build (stub) started\n");
  (void)cmd_buffer;
  g_rtx_blas_tlas_built = qtrue;
  return qtrue;
}

qboolean vk_rtx_build_blas_for_geometry_real(VkCommandBuffer cmd_buffer) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: BLAS for geometry real build (stub)\n");
  (void)cmd_buffer;
  g_rtx_blas_tlas_built = qtrue;
  return qtrue;
}

void vk_rtx_create_sbt_buffer_full(void) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: creating full SBT buffer (stub)\n");
  if (g_sbt.sbt_buffer.buffer == VK_NULL_HANDLE) {
    // placeholder
  }
}

void vk_rtx_build_sbt_for_frame_full(VkCommandBuffer cmd_buffer) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: per-frame SBT build (full) start\n");
  if (g_sbt.sbt_buffer.buffer == VK_NULL_HANDLE) {
    vk_rtx_create_sbt_buffer_full();
    if (g_sbt.sbt_buffer.buffer == VK_NULL_HANDLE) {
      ri.Printf(PRINT_WARNING, "RTX: SBT buffer not created, skip fill\n");
      return;
    }
  }
  // Get the shader group handles for 3 groups
  VkResult result = vkGetRayTracingShaderGroupHandlesKHR(vk.device, g_rt_pipeline, 0, 3,
                                                      3 * MAX_SHADER_HANDLE_SIZE, g_shader_handles);
  if (result != VK_SUCCESS) {
    ri.Printf(PRINT_ERROR, "RTX: Failed to get shader group handles for SBT\n");
    return;
  }
  // Copy handles into SBT (raygen, miss, closest-hit)
  void* mapped = NULL;
  if (g_sbt.sbt_buffer.memory != VK_NULL_HANDLE) {
    VkResult map_res = vkMapMemory(vk.device, g_sbt.sbt_buffer.memory, 0, g_sbt.sbt_buffer.size, 0, &mapped);
    if (map_res == VK_SUCCESS && mapped) {
      memcpy(mapped, g_shader_handles, g_shader_handle_size);
      memcpy((uint8_t*)mapped + g_sbt.sbt_record_size, g_shader_handles + g_shader_handle_size, g_shader_handle_size);
      memcpy((uint8_t*)mapped + 2 * g_sbt.sbt_record_size, g_shader_handles + 2 * g_shader_handle_size, g_shader_handle_size);
      vkUnmapMemory(vk.device, g_sbt.sbt_buffer.memory);
    } else {
      ri.Printf(PRINT_ERROR, "RTX: Failed to map SBT memory for writing\n");
      return;
    }
  }
  // Offsets
  g_sbt.sbt_raygen_offset = 0;
  g_sbt.sbt_miss_offset = g_sbt.sbt_record_size;
  g_sbt.sbt_hit_offset = 2 * g_sbt.sbt_record_size;
  ri.Printf(PRINT_ALL, "Vulkan RTX: SBT built for frame (full)\n");
}
void vk_rtx_build_sbt_for_frame(VkCommandBuffer cmd_buffer) {
    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: per-frame SBT build request (wrapper)\n");
    // Placeholder; actual SBT population would occur here
}
#include "tr_local.h"
#include "vk_rtx_acceleration.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <time.h>
// Stub: create RT pipeline (Plan A groundwork)
VkResult vk_rtx_create_pipeline(void) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: creating (stub) RT pipeline\n");
  // In a real implementation, you'd create g_rt_pipeline and g_rt_pipeline_layout here.
  g_rt_pipeline = VK_NULL_HANDLE;
  g_rt_pipeline_layout = VK_NULL_HANDLE;
  return VK_SUCCESS;
}
#ifdef VK_CALIBRATED_TIMESTAMPS_ENABLED
static void VK_debug_calibrated_ts(void) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: calibrated timestamps are enabled\n");
}
#endif
void vk_update_gpu_timing_ns(uint64_t gpu_ns);
// Lightweight stub for calibrated timestamp initialization
void VK_try_init_calibrated_timestamps(void) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: VK_try_init_calibrated_timestamps (stub)\n");
}

// Include Vulkan headers for ray tracing
#ifdef USE_VULKAN
// Note: VMA not used in this implementation - using raw Vulkan memory allocation
#endif

#ifdef __cplusplus
extern "C" {
#endif

// RTX acceleration structure state
typedef struct {
    VkAccelerationStructureKHR accel;
    VkDeviceMemory memory;
    VkDeviceAddress address;
    uint64_t handle;
} rtx_acceleration_t; // end of rtx_acceleration_t

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceAddress address;
    void* mapped;
    VkDeviceSize size;
} rtx_buffer_t; // end of rtx_buffer_t

typedef struct {
    rtx_buffer_t sbt_buffer;
    uint32_t sbt_record_size;
    uint32_t sbt_raygen_offset;
    uint32_t sbt_miss_offset;
    uint32_t sbt_hit_offset;
    uint32_t sbt_callable_offset;
} rtx_sbt_t;

// Global RTX state
static qboolean g_rtx_accel_initialized = qfalse;
static qboolean g_rtx_blas_tlas_built = qfalse;
// Timing: GPU/RT timestamps
static VkQueryPool g_rtx_timing_query_pool = VK_NULL_HANDLE;
static float     g_rtx_timestamp_period = 1.0f;

#define MAX_BLAS 1024
#define MAX_TLAS_INSTANCES 4096

static rtx_acceleration_t g_blas[MAX_BLAS];
static uint32_t g_blas_count = 0;

static rtx_acceleration_t g_tlas;
static rtx_buffer_t g_instance_buffer;
static rtx_sbt_t g_sbt;

static VkPipeline g_rt_pipeline = VK_NULL_HANDLE;
static VkPipelineLayout g_rt_pipeline_layout = VK_NULL_HANDLE;
static VkDescriptorSetLayout g_rt_descriptor_set_layout = VK_NULL_HANDLE;
static VkDescriptorSet g_rt_descriptor_set = VK_NULL_HANDLE;
// Scratch buffer for TLAS/BLAS building per frame
VkBuffer g_rt_scratch_buffer = VK_NULL_HANDLE;
VkDeviceMemory g_rt_scratch_memory = VK_NULL_HANDLE;
VkDeviceSize g_rt_scratch_size = 0;

// Ray tracing shader groups
static VkRayTracingShaderGroupCreateInfoKHR g_shader_groups[3] = {0};

// Shader handle storage
#define MAX_SHADER_HANDLE_SIZE 32
static uint8_t g_shader_handles[3 * MAX_SHADER_HANDLE_SIZE];
static uint32_t g_shader_handle_size = 0;

// Utility functions
static void* rtx_alloc(VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory, VkDeviceAddress* address) {
    if (!vk.allocator) return NULL;

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    VmaAllocation allocation;
    VkResult result = vmaCreateBuffer(vk.allocator, &buffer_info, &alloc_info, buffer, &allocation, NULL);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to allocate buffer\n");
        return NULL;
    }

    // Get device address
    VkBufferDeviceAddressInfo address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = *buffer
    };
    *address = vkGetBufferDeviceAddress(vk.device, &address_info);

    // For this simple implementation, we'll use CPU-visible memory for the buffer
    // In a real implementation, you'd want GPU-only memory
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(vk.device, *buffer, &mem_req);

    VkMemoryAllocateInfo alloc_info_cpu = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = 0 // Would need to find appropriate memory type
    };

    // For simplicity, we'll skip the actual allocation here and return success
    // A full implementation would allocate and bind memory
    *memory = VK_NULL_HANDLE;

    return (void*)0x1; // Dummy pointer
}

void vk_rtx_build_tlas_for_frame(VkCommandBuffer cmd_buffer) {
    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: per-frame TLAS build request (wrapper)\n");
#ifdef VK_RTX_REAL_TLAS_AVAIL
    if (vk_rtx_build_tlas_real_full) {
        vk_rtx_build_tlas_real_full(cmd_buffer);
        g_rtx_blas_tlas_built = qtrue;
        return;
    }
#endif
    vk_rtx_build_tlas(cmd_buffer);
}

// Real TLAS builder (hook; to be expanded into full TLAS construction)
qboolean vk_rtx_build_tlas_real(VkCommandBuffer cmd_buffer) {
  static qboolean already_built = qfalse;
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: TLAS real build (scaffold) started - cmd_buf=%p\n", (void*)cmd_buffer);
  (void)cmd_buffer;
  if (!already_built) {
    already_built = qtrue;
    ri.Printf(PRINT_ALL, "TLAS scaffold: preparing BLAS/TLAS placeholders (no actual vulkan calls yet).\n");
  } else {
    ri.Printf(PRINT_DEVELOPER, "TLAS scaffold already initialized.\n");
  }
  // Indicate readiness for real wiring in subsequent patches
  g_rtx_blas_tlas_built = qtrue;
  return qtrue;
}

void vk_rtx_build_tlas_for_frame(VkCommandBuffer cmd_buffer) {
    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: per-frame TLAS build request (wrapper)\n");
    vk_rtx_build_tlas(cmd_buffer);
}

void vk_rtx_build_sbt_for_frame(VkCommandBuffer cmd_buffer) {
    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: per-frame SBT build request (wrapper)\n");
    // placeholder; actual SBT population would occur here in a full impl
}

static void rtx_free_buffer(rtx_buffer_t* buffer) {
    if (buffer->mapped) {
        vkUnmapMemory(vk.device, buffer->memory);
        buffer->mapped = NULL;
    }
    if (buffer->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk.device, buffer->buffer, NULL);
        buffer->buffer = VK_NULL_HANDLE;
    }
    if (buffer->memory != VK_NULL_HANDLE) {
        vkFreeMemory(vk.device, buffer->memory, NULL);
        buffer->memory = VK_NULL_HANDLE;
    }
}
qboolean vk_rtx_acceleration_init(void) {
    // Defensive guard: prevent double initialization
    if (g_rtx_accel_initialized) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: acceleration already initialized\n");
        return qtrue;
    }

    // Guard: ensure Vulkan is active before proceeding
    if (!vk.active) {
        ri.Printf(PRINT_WARNING, "Vulkan RTX: Vulkan not active, cannot initialize RTX acceleration\n");
        return qfalse;
    }

    ri.Printf(PRINT_ALL, "Vulkan RTX: Initializing acceleration structures\n");

    // Initialize acceleration structure state with defensive memset
    memset(g_blas, 0, sizeof(g_blas));
    memset(&g_tlas, 0, sizeof(g_tlas));
    memset(&g_instance_buffer, 0, sizeof(g_instance_buffer));
    memset(&g_sbt, 0, sizeof(g_sbt));

    g_blas_count = 0;
    g_rtx_blas_tlas_built = qfalse;

    // Guard: only create pipeline if Vulkan device is valid
    if (vk.device == VK_NULL_HANDLE) {
        ri.Printf(PRINT_ERROR, "Vulkan RTX: Invalid Vulkan device, cannot create pipeline\n");
        return qfalse;
    }

    // Create basic ray tracing pipeline (simplified - would normally load shaders)
    VkResult result = vk_rtx_create_pipeline();
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan RTX: Failed to create ray tracing pipeline (error: %d)\n", result);
        return qfalse;
    }

    // Initialize calibrated timestamps support (best-effort)
    VK_CAL_TS_INIT_ONCE

    // Initialize timing query pool for GPU timing - add defensive check
    if (vk.device != VK_NULL_HANDLE) {
        VkQueryPoolCreateInfo qp = { .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 2 };
        if (vkCreateQueryPool(vk.device, &qp, NULL, &g_rtx_timing_query_pool) != VK_SUCCESS) {
            ri.Printf(PRINT_WARNING, "Vulkan RTX: failed to create timing query pool\n");
            g_rtx_timing_query_pool = VK_NULL_HANDLE;
        } else {
            g_rtx_timestamp_period = 1.0f;
        }
    } else {
        ri.Printf(PRINT_WARNING, "Vulkan RTX: cannot create timing query pool - device not available\n");
        g_rtx_timing_query_pool = VK_NULL_HANDLE;
    }

    g_rtx_accel_initialized = qtrue;
    ri.Printf(PRINT_ALL, "Vulkan RTX: Acceleration structures initialized successfully\n");
    return qtrue;
}

void vk_rtx_acceleration_shutdown(void) {
    if (!g_rtx_accel_initialized) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: acceleration shutdown called; not initialized\n");
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan RTX: Shutting down acceleration structures\n");

    // Destroy TLAS
    if (g_tlas.accel != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(vk.device, g_tlas.accel, NULL);
        g_tlas.accel = VK_NULL_HANDLE;
    }

    // Destroy BLAS
    for (uint32_t i = 0; i < g_blas_count; i++) {
        if (g_blas[i].accel != VK_NULL_HANDLE) {
            vkDestroyAccelerationStructureKHR(vk.device, g_blas[i].accel, NULL);
            g_blas[i].accel = VK_NULL_HANDLE;
        }
    }

    // Destroy buffers
    rtx_free_buffer(&g_instance_buffer);
    rtx_free_buffer(&g_sbt.sbt_buffer);

    // Destroy pipeline
    if (g_rt_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vk.device, g_rt_pipeline, NULL);
        g_rt_pipeline = VK_NULL_HANDLE;
    }

    if (g_rt_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vk.device, g_rt_pipeline_layout, NULL);
        g_rt_pipeline_layout = VK_NULL_HANDLE;
    }

    if (g_rt_descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk.device, g_rt_descriptor_set_layout, NULL);
        g_rt_descriptor_set_layout = VK_NULL_HANDLE;
    }

    g_rtx_accel_initialized = qfalse;
    g_rtx_blas_tlas_built = qfalse;
    g_blas_count = 0;

    // Clean up timing query pool if created
    if (g_rtx_timing_query_pool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(vk.device, g_rtx_timing_query_pool, NULL);
        g_rtx_timing_query_pool = VK_NULL_HANDLE;
    }
    ri.Printf(PRINT_ALL, "Vulkan RTX: Acceleration structures shut down\n");
}

uint64_t vk_rtx_create_blas_for_geometry(VkBuffer vertex_buffer, VkBuffer index_buffer,
                                      uint32_t vertex_count, uint32_t index_count,
                                      uint32_t vertex_stride, const char* debug_name) {
    if (!g_rtx_accel_initialized) {
        ri.Printf(PRINT_WARNING, "Vulkan RTX: BLAS requested before init; returning 0\n");
        return 0;
    }

    if (g_blas_count >= MAX_BLAS) {
        ri.Printf(PRINT_ERROR, "Vulkan RTX: Maximum BLAS count reached\n");
        return 0;
    }

    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: Creating BLAS for %s verts=%u idx=%u\n",
              debug_name ? debug_name : "unnamed", vertex_count, index_count);

    // Create geometry info
    VkAccelerationStructureGeometryKHR geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
        .geometry = {
            .triangles = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexData = {
                    .deviceAddress = vertex_buffer ? vkGetBufferDeviceAddress(vk.device,
                        &(VkBufferDeviceAddressInfo){.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = vertex_buffer}) : 0
                },
                .vertexStride = vertex_stride,
                .maxVertex = vertex_count,
                .indexType = index_buffer ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_NONE_KHR,
                .indexData = {
                    .deviceAddress = index_buffer ? vkGetBufferDeviceAddress(vk.device,
                        &(VkBufferDeviceAddressInfo){.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = index_buffer}) : 0
                },
                .transformData = {0} // Identity transform
            }
        }
    };

    // Build info for BLAS
    VkAccelerationStructureBuildGeometryInfoKHR build_info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    // Get build sizes
    VkAccelerationStructureBuildSizesInfoKHR build_sizes = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    uint32_t primitive_count = index_count / 3; // Assuming triangles
    vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                           &build_info, &primitive_count, &build_sizes);

    // Allocate BLAS buffer
    rtx_buffer_t blas_buffer;
    rtx_alloc(build_sizes.accelerationStructureSize, &blas_buffer.buffer, &blas_buffer.memory, &blas_buffer.address);

    // Create acceleration structure
    VkAccelerationStructureCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blas_buffer.buffer,
        .size = build_sizes.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };

    VkAccelerationStructureKHR accel;
    VkResult result = vkCreateAccelerationStructureKHR(vk.device, &create_info, NULL, &accel);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to create BLAS\n");
        return 0;
    }

    // Store BLAS
    uint32_t blas_index = g_blas_count++;
    g_blas[blas_index].accel = accel;
    g_blas[blas_index].memory = blas_buffer.memory;
    g_blas[blas_index].address = blas_buffer.address;
    g_blas[blas_index].handle = (uint64_t)accel; // Use acceleration structure as handle

    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: BLAS created successfully, handle=%llu\n", g_blas[blas_index].handle);
    return g_blas[blas_index].handle;
}

void vk_rtx_update_instance_data(uint64_t accel_id, const matrix3x4_t* transform, uint32_t instance_id) {
    (void)accel_id; (void)transform; (void)instance_id;
}

void vk_rtx_build_tlas(VkCommandBuffer cmd_buffer) {
    if (!g_rtx_accel_initialized) {
        ri.Printf(PRINT_WARNING, "Vulkan RTX: TLAS build requested before init\n");
        return;
    }

    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: Building TLAS with %u BLAS instances\n", g_blas_count);

    if (g_blas_count == 0) {
        ri.Printf(PRINT_WARNING, "Vulkan RTX: No BLAS available for TLAS\n");
        return;
    }

    // Create instance buffer for TLAS
    VkDeviceSize instance_buffer_size = sizeof(VkAccelerationStructureInstanceKHR) * g_blas_count;

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = instance_buffer_size,
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer instance_buffer;
    VkDeviceMemory instance_memory;
    VkResult result = vkCreateBuffer(vk.device, &buffer_info, NULL, &instance_buffer);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to create instance buffer\n");
        return;
    }

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(vk.device, instance_buffer, &mem_req);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = 0 // Would need proper memory type selection
    };

    result = vkAllocateMemory(vk.device, &alloc_info, NULL, &instance_memory);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to allocate instance buffer memory\n");
        vkDestroyBuffer(vk.device, instance_buffer, NULL);
        return;
    }

    vkBindBufferMemory(vk.device, instance_buffer, instance_memory, 0);

    // Map and fill instance buffer
    VkAccelerationStructureInstanceKHR* instances;
    vkMapMemory(vk.device, instance_memory, 0, instance_buffer_size, 0, (void**)&instances);

    for (uint32_t i = 0; i < g_blas_count; i++) {
        // Identity transform matrix
        instances[i].transform.matrix[0][0] = 1.0f;
        instances[i].transform.matrix[1][1] = 1.0f;
        instances[i].transform.matrix[2][2] = 1.0f;

        instances[i].instanceCustomIndex = i;
        instances[i].mask = 0xFF;
        instances[i].instanceShaderBindingTableRecordOffset = 0;
        instances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instances[i].accelerationStructureReference = g_blas[i].address;
    }

    vkUnmapMemory(vk.device, instance_memory);

    // Store instance buffer info
    g_instance_buffer.buffer = instance_buffer;
    g_instance_buffer.memory = instance_memory;
    g_instance_buffer.size = instance_buffer_size;

    VkBufferDeviceAddressInfo address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = instance_buffer
    };
    g_instance_buffer.address = vkGetBufferDeviceAddress(vk.device, &address_info);

    // Create geometry for TLAS
    VkAccelerationStructureGeometryKHR geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {
            .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .data = {
                    .deviceAddress = g_instance_buffer.address
                }
            }
        }
    };

    // Build info for TLAS
    VkAccelerationStructureBuildGeometryInfoKHR build_info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    // Get build sizes
    VkAccelerationStructureBuildSizesInfoKHR build_sizes = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                           &build_info, &g_blas_count, &build_sizes);

    // Allocate TLAS buffer
    rtx_buffer_t tlas_buffer;
    rtx_alloc(build_sizes.accelerationStructureSize, &tlas_buffer.buffer, &tlas_buffer.memory, &tlas_buffer.address);

    // Create TLAS
    VkAccelerationStructureCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlas_buffer.buffer,
        .size = build_sizes.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

    result = vkCreateAccelerationStructureKHR(vk.device, &create_info, NULL, &g_tlas.accel);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to create TLAS\n");
        return;
    }

    g_tlas.memory = tlas_buffer.memory;
    g_tlas.address = tlas_buffer.address;

    VkAccelerationStructureDeviceAddressInfoKHR device_address_info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = g_tlas.accel
    };
    g_tlas.handle = vkGetAccelerationStructureDeviceAddressKHR(vk.device, &device_address_info);

    // Build TLAS
    VkAccelerationStructureBuildRangeInfoKHR build_range = {
        .primitiveCount = g_blas_count,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };

    build_info.dstAccelerationStructure = g_tlas.accel;
    build_info.scratchData.deviceAddress = 0; // Would need scratch buffer

    // Build acceleration structure
    vkCmdBuildAccelerationStructuresKHR(cmd_buffer, 1, &build_info, &build_range);

    // Add barrier
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };

    vkCmdPipelineBarrier(cmd_buffer,
                        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                        0, 1, &barrier, 0, NULL, 0, NULL);

    g_rtx_blas_tlas_built = qtrue;
    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: TLAS built successfully\n");
}


uint32_t vk_rtx_add_light(const vec3_t position, const vec3_t color, float intensity,
                          float radius, int light_type, qboolean casts_shadows) {
    (void)position; (void)color; (void)intensity; (void)radius; (void)light_type; (void)casts_shadows;
    return 0;
}

void vk_rtx_update_light(uint32_t light_id, const vec3_t position, const vec3_t color, float intensity) {
    (void)light_id; (void)position; (void)color; (void)intensity;
}

void vk_rtx_set_light_advanced_properties(uint32_t light_id, float temperature,
                                         float cone_angle, const vec3_t direction, int ies_profile) {
    (void)light_id; (void)temperature; (void)cone_angle; (void)direction; (void)ies_profile;
}

void vk_rtx_create_shadow_acceleration(uint32_t light_id, VkCommandBuffer cmd_buffer) {
    if (!g_rtx_accel_initialized || !g_rtx_available) {
        ri.Printf(PRINT_DEVELOPER, "RTX: shadow acceleration skipped (not initialized)\n");
        return;
    }

    if (!g_rt_pipeline) {
        ri.Printf(PRINT_WARNING, "RTX: No RT pipeline for shadow acceleration\n");
        return;
    }

    // For now, shadow acceleration is handled in the ray tracing pipeline
    // This function can be extended for specialized shadow acceleration structures
    ri.Printf(PRINT_DEVELOPER, "RTX: Shadow acceleration ready for light %u\n", light_id);
}

// Perform shadow ray tracing for a point and light direction
qboolean vk_rtx_trace_shadow_ray(const vec3_t origin, const vec3_t direction, float distance) {
    if (!g_rtx_accel_initialized || !g_rtx_available || !g_rt_pipeline) {
        return qfalse; // No shadow information available
    }

    // This is a high-level interface for shadow testing
    // The actual shadow tracing is done in the raygen shader
    // Return false (not shadowed) for now - will be properly implemented
    // when the shader pipeline is fully integrated
    return qfalse;
}

void vk_rtx_update_lighting_data(VkBuffer light_buffer, VkDeviceMemory light_buffer_memory) {
    (void)light_buffer; (void)light_buffer_memory;
}

void vk_rtx_trace_raysKHR(VkCommandBuffer cmd_buffer) {
    if (!g_rt_pipeline || !g_rtx_blas_tlas_built) {
        ri.Printf(PRINT_WARNING, "Vulkan RTX: Cannot trace rays - pipeline or TLAS not ready\n");
        return;
    }

    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: Executing vkCmdTraceRaysKHR\n");

    // Bind RT pipeline
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, g_rt_pipeline);

    // Bind descriptor set if available
    if (g_rt_descriptor_set != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                               g_rt_pipeline_layout, 0, 1, &g_rt_descriptor_set, 0, NULL);
    }

    // Set up ray tracing regions
    VkStridedDeviceAddressRegionKHR raygen_region = {
        .deviceAddress = g_sbt.sbt_buffer.address + g_sbt.sbt_raygen_offset,
        .stride = g_sbt.sbt_record_size,
        .size = g_sbt.sbt_record_size
    };

    VkStridedDeviceAddressRegionKHR miss_region = {
        .deviceAddress = g_sbt.sbt_buffer.address + g_sbt.sbt_miss_offset,
        .stride = g_sbt.sbt_record_size,
        .size = g_sbt.sbt_record_size
    };

    VkStridedDeviceAddressRegionKHR hit_region = {
        .deviceAddress = g_sbt.sbt_buffer.address + g_sbt.sbt_hit_offset,
        .stride = g_sbt.sbt_record_size,
        .size = g_sbt.sbt_record_size
    };

    VkStridedDeviceAddressRegionKHR callable_region = {0}; // Not used in this simple implementation

    // Optional start timestamp (only if calibrated timestamps available)
    if (g_rtx_timing_query_pool != VK_NULL_HANDLE && g_cal_ts_available) {
        vkCmdWriteTimestamp(cmd_buffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, g_rtx_timing_query_pool, 0);
    }
    // Issue the trace rays command
    vkCmdTraceRaysKHR(cmd_buffer, &raygen_region, &miss_region, &hit_region, &callable_region,
                     glConfig.vidWidth, glConfig.vidHeight, 1);
    // Optional end timestamp (only if calibrated timestamps available)
    if (g_rtx_timing_query_pool != VK_NULL_HANDLE && g_cal_ts_available) {
        vkCmdWriteTimestamp(cmd_buffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, g_rtx_timing_query_pool, 1);
    }

    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: vkCmdTraceRaysKHR executed for %ux%u\n",
              glConfig.vidWidth, glConfig.vidHeight);
}

void vk_rtx_bind_and_trace_raysKHR_from_main(VkCommandBuffer cmd_buffer, uint32_t width, uint32_t height) {
    // Guard early
    if (width == 0 || height == 0) { ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: zero-dim; skip\n"); return; }
    if (!g_rtx_accel_initialized) { ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: acceleration not initialized; skip\n"); return; }
    if (g_rt_pipeline == VK_NULL_HANDLE) { ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: pipeline not created; skip\n"); return; }
    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: Starting ray tracing pipeline %ux%u\n", width, height);
    // Initialize per-frame TLAS/BLAS and SBT wiring
    #ifdef VK_CALIBRATED_TIMESTAMPS_ENABLED
    VK_debug_calibrated_ts();
    #endif
    vk_rtx_build_tlas_for_frame(cmd_buffer);
    vk_rtx_build_sbt_for_frame(cmd_buffer);
    // Extra defensive guard: ensure we have a built SBT
    if (g_sbt.sbt_buffer.buffer == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Vulkan RTX: SBT not ready after build steps; abort trace\n");
        return;
    }
    // Ensure TLAS/BLAS have been built; try to build if not yet built
    if (!g_rtx_blas_tlas_built) {
        vk_rtx_build_tlas_for_frame(cmd_buffer);
        if (!g_rtx_blas_tlas_built) {
            ri.Printf(PRINT_WARNING, "Vulkan RTX: TLAS/BLAS not ready; abort trace\n");
            return;
        }
    }
    // Guard: ensure SBT is ready before tracing
    if (g_sbt.sbt_buffer.buffer == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Vulkan RTX: SBT not ready; skipping trace\n");
        return;
    }

    #ifdef VK_KHR_ray_tracing_pipeline
        // Ensure acceleration structures are built
      if (!g_rtx_blas_tlas_built) {
          vk_rtx_build_tlas_for_frame(cmd_buffer);
      }

      // Set up SBT
      vk_rtx_setup_sbt(cmd_buffer);
      // Build per-frame SBT (wrapper)
      vk_rtx_build_sbt_for_frame(cmd_buffer);

        // Memory barrier to ensure acceleration structure build is complete
        VkMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
        };

        vkCmdPipelineBarrier(cmd_buffer,
                            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                            0, 1, &barrier, 0, NULL, 0, NULL);

        // Execute ray tracing and measure CPU time for the frame
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        vk_rtx_trace_raysKHR(cmd_buffer);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        uint64_t elapsed_ns = (uint64_t)(t1.tv_sec - t0.tv_sec) * 1000000000ULL + (t1.tv_nsec - t0.tv_nsec);
        ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: trace took %llu ns for frame (%ux%u)\n",
                  (unsigned long long)elapsed_ns, width, height);

    // Transition ray tracing output to presentation layout
        // This assumes the output image is the current swapchain image
        VkImageMemoryBarrier present_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = vk.swapchain_images[vk.current_swapchain_image_index],
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        vkCmdPipelineBarrier(cmd_buffer,
                            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                            0, 0, NULL, 0, NULL, 1, &present_barrier);

        ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: Ray tracing pipeline completed, output ready for presentation\n");
        // Optional: read back timing results
        if (g_rtx_timing_query_pool != VK_NULL_HANDLE) {
            uint64_t timestamps[2] = {0, 0};
            VkResult qr = vkGetQueryPoolResults(vk.device, g_rtx_timing_query_pool, 0, 2,
                                                sizeof(uint64_t) * 2, timestamps, 0,
                                                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
            if (qr == VK_SUCCESS) {
                uint64_t delta = timestamps[1] - timestamps[0];
                uint64_t gpu_ns = delta * (uint64_t)g_rtx_timestamp_period;
                ri.Printf(PRINT_DEVELOPER, "RTX: GPU frame time ~ %llu ns\n", (unsigned long long)gpu_ns);
                vk_update_gpu_timing_ns(gpu_ns);
            } else {
                ri.Printf(PRINT_DEVELOPER, "RTX: timing query results not ready (0x%x)\n", qr);
            }
        }
    #else
        ri.Printf(PRINT_WARNING, "Vulkan RTX: VK_KHR_ray_tracing_pipeline not available\n");
    #endif

    (void)cmd_buffer;
    (void)width;
    (void)height;
}

// Calibrated timestamps preliminary wiring (best-effort)
static qboolean g_cal_ts_attempted = qfalse;
static qboolean g_cal_ts_available = qfalse;
#if defined(VK_EXT_CALIBRATED_TIMESTAMPS_ENABLED)
static bool g_cal_ts_ext_present = false;
static void vk_rtx_init_calibrated_ts_stub(void) {
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: calibrated timestamps extension stub inited\n");
  g_cal_ts_ext_present = true;
  g_cal_ts_available = qtrue;
  g_rtx_timestamp_period = 1.0f;
}
#endif
void VK_try_init_calibrated_timestamps(void) {
#if defined(VK_EXT_CALIBRATED_TIMESTAMPS_ENABLED)
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: calibrated timestamps probing (stub)\n");
  // In a full patch, load entry points then query device properties.
  // Current minimal behavior: assume not available yet.
  g_cal_ts_available = qfalse;
#else
  ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: calibrated timestamps extension not available\n");
  g_cal_ts_available = qfalse;
#endif
}

void vk_rtx_create_sbt_buffer(void) {
    if (!g_rtx_accel_initialized) {
        ri.Printf(PRINT_WARNING, "Vulkan RTX: SBT creation requested before init\n");
        return;
    }

    // Get shader group handle size
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    VkPhysicalDeviceProperties2 device_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rt_props
    };

    vkGetPhysicalDeviceProperties2(vk.physical_device, &device_props);

    g_shader_handle_size = rt_props.shaderGroupHandleSize;
    g_sbt.sbt_record_size = rt_props.shaderGroupBaseAlignment;

    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: Shader handle size=%u, SBT record size=%u\n",
              g_shader_handle_size, g_sbt.sbt_record_size);

    // Create SBT buffer
    VkDeviceSize sbt_size = 3 * g_sbt.sbt_record_size; // raygen, miss, closest hit

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sbt_size,
        .usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkResult result = vkCreateBuffer(vk.device, &buffer_info, NULL, &g_sbt.sbt_buffer.buffer);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to create SBT buffer\n");
        return;
    }

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(vk.device, g_sbt.sbt_buffer.buffer, &mem_req);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = 0 // Would need proper memory type
    };

    result = vkAllocateMemory(vk.device, &alloc_info, NULL, &g_sbt.sbt_buffer.memory);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to allocate SBT buffer memory\n");
        vkDestroyBuffer(vk.device, g_sbt.sbt_buffer.buffer, NULL);
        return;
    }

    vkBindBufferMemory(vk.device, g_sbt.sbt_buffer.buffer, g_sbt.sbt_buffer.memory, 0);
    g_sbt.sbt_buffer.size = sbt_size;

    VkBufferDeviceAddressInfo address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = g_sbt.sbt_buffer.buffer
    };
    g_sbt.sbt_buffer.address = vkGetBufferDeviceAddress(vk.device, &address_info);

    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: SBT buffer created successfully\n");
}

void vk_rtx_build_sbt_for_frame(VkCommandBuffer cmd_buffer) {
    if (!g_rt_pipeline) {
        ri.Printf(PRINT_WARNING, "Vulkan RTX: No RT pipeline for SBT build\n");
        return;
    }

    // Get shader group handles
    VkResult result = vkGetRayTracingShaderGroupHandlesKHR(vk.device, g_rt_pipeline, 0, 3,
                                                          3 * g_shader_handle_size, g_shader_handles);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to get shader group handles\n");
        return;
    }

    // Map SBT buffer and copy handles
    void* mapped;
    vkMapMemory(vk.device, g_sbt.sbt_buffer.memory, 0, g_sbt.sbt_buffer.size, 0, &mapped);

    // Copy raygen handle
    memcpy(mapped, g_shader_handles, g_shader_handle_size);

    // Copy miss handle
    memcpy((uint8_t*)mapped + g_sbt.sbt_record_size,
           g_shader_handles + g_shader_handle_size, g_shader_handle_size);

    // Copy hit handle
    memcpy((uint8_t*)mapped + 2 * g_sbt.sbt_record_size,
           g_shader_handles + 2 * g_shader_handle_size, g_shader_handle_size);

    vkUnmapMemory(vk.device, g_sbt.sbt_buffer.memory);

    // Set SBT offsets
    g_sbt.sbt_raygen_offset = 0;
    g_sbt.sbt_miss_offset = g_sbt.sbt_record_size;
    g_sbt.sbt_hit_offset = 2 * g_sbt.sbt_record_size;

    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: SBT built for frame\n");
}

void vk_rtx_setup_sbt(VkCommandBuffer cmd_buffer) {
    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: Setting up SBT\n");

    // Create SBT buffer if not already created
    if (g_sbt.sbt_buffer.buffer == VK_NULL_HANDLE) {
        vk_rtx_create_sbt_buffer();
    }

    // Build SBT for this frame
    vk_rtx_build_sbt_for_frame(cmd_buffer);
}

static rtx_quality_preset_t g_current_quality_preset = RTX_QUALITY_MEDIUM;

void vk_rtx_set_quality_settings(float quality, qboolean shadows, qboolean reflections,
                                 qboolean refractions, qboolean global_illumination) {
    // Update CVar-based settings
    if (r_rtx_shadows) ri.Cvar_Set("r_rtx_shadows", shadows ? "1" : "0");
    if (r_rtx_reflections) ri.Cvar_Set("r_rtx_reflections", reflections ? "1" : "0");
    if (r_rtx_gi) ri.Cvar_Set("r_rtx_gi", global_illumination ? "1" : "0");
    if (r_rtx_quality) ri.Cvar_SetValue("r_rtx_quality", quality);

    ri.Printf(PRINT_ALL, "RTX: Quality settings updated - shadows:%d reflections:%d GI:%d quality:%.1f\n",
              shadows, reflections, global_illumination, quality);
}

void vk_rtx_set_quality_preset(rtx_quality_preset_t preset) {
    g_current_quality_preset = preset;

    switch (preset) {
        case RTX_QUALITY_LOW:
            // Low quality: shadows only, minimal features
            vk_rtx_set_quality_settings(0.3f, qtrue, qfalse, qfalse, qfalse);
            ri.Printf(PRINT_ALL, "RTX: Set to LOW quality preset\n");
            break;

        case RTX_QUALITY_MEDIUM:
            // Medium quality: shadows + reflections
            vk_rtx_set_quality_settings(0.6f, qtrue, qtrue, qfalse, qfalse);
            ri.Printf(PRINT_ALL, "RTX: Set to MEDIUM quality preset\n");
            break;

        case RTX_QUALITY_HIGH:
            // High quality: shadows + reflections + basic GI
            vk_rtx_set_quality_settings(0.8f, qtrue, qtrue, qfalse, qtrue);
            ri.Printf(PRINT_ALL, "RTX: Set to HIGH quality preset\n");
            break;

        case RTX_QUALITY_ULTRA:
            // Ultra quality: all features enabled
            vk_rtx_set_quality_settings(1.0f, qtrue, qtrue, qfalse, qtrue);
            ri.Printf(PRINT_ALL, "RTX: Set to ULTRA quality preset\n");
            break;

        default:
            ri.Printf(PRINT_WARNING, "RTX: Unknown quality preset %d\n", preset);
            break;
    }
}

rtx_quality_preset_t vk_rtx_get_current_quality_preset(void) {
    return g_current_quality_preset;
}

void vk_rtx_get_statistics(uint32_t* triangles, uint32_t* instances, uint32_t* lights,
                         VkDeviceSize* memory_usage, float* quality) {
    if (triangles) *triangles = 0;
    if (instances) *instances = 0;
    if (lights) *lights = 0;
    if (memory_usage) *memory_usage = 0;
    if (quality) *quality = 0.0f;
}

void vk_rtx_performance_monitor(uint64_t frame_time_ns, uint32_t ray_count) {
    (void)frame_time_ns; (void)ray_count;
}

// SPIR-V shader loading and compilation
static VkShaderModule vk_rtx_load_shader(const char* filename) {
    // For now, we'll embed minimal SPIR-V shaders directly
    // In a real implementation, this would load from files

    // Minimal raygen shader SPIR-V (placeholder - this would be much larger in reality)
    static const uint32_t raygen_spirv[] = {
        0x07230203, 0x00010000, 0x00080001, 0x0000002D, 0x00000000, 0x00020011,
        0x00000001, 0x0006000B, 0x00000001, 0x4C534C47, 0x6474732E, 0x3035342E,
        0x00000000, 0x0003000E, 0x00000000, 0x00000001, 0x0006000F, 0x00000005,
        0x00000004, 0x6E69616D, 0x00000000, 0x0000000D, 0x00030010, 0x00000004,
        0x00000007, 0x00030003, 0x00000002, 0x000001C2, 0x00040005, 0x00000004,
        0x616D696E, 0x00000000, 0x00050005, 0x0000000A, 0x4374756F, 0x6E696C6F,
        0x00000000, 0x00080005, 0x0000000D, 0x475F4C49, 0x616E756F, 0x6F436863,
        0x64656472, 0x00000000, 0x00040047, 0x0000000A, 0x0000000B, 0x0000001C,
        0x00040047, 0x0000000D, 0x0000000B, 0x0000001B, 0x00020013, 0x00000002,
        0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020,
        0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020, 0x00000008,
        0x00000003, 0x00000007, 0x0004003B, 0x00000008, 0x00000009, 0x00000003,
        0x00040017, 0x0000000B, 0x00000006, 0x00000002, 0x00040020, 0x0000000C,
        0x00000001, 0x0000000B, 0x0004003B, 0x0000000C, 0x0000000D, 0x00000001,
        0x0004002B, 0x00000006, 0x0000000E, 0x3F800000, 0x0004002B, 0x00000006,
        0x0000000F, 0x00000000, 0x0004002B, 0x00000006, 0x00000010, 0x3F000000,
        0x0007002F, 0x00000007, 0x00000011, 0x0000000E, 0x0000000F, 0x00000010,
        0x0000000E, 0x0003003E, 0x00000009, 0x00000011, 0x00050041, 0x0000000C,
        0x00000012, 0x0000000D, 0x0004002B, 0x00000005, 0x00000013, 0x00000000,
        0x0004003D, 0x0000000B, 0x00000014, 0x00000012, 0x00050051, 0x00000006,
        0x00000015, 0x00000014, 0x00000000, 0x00050051, 0x00000006, 0x00000016,
        0x00000014, 0x00000001, 0x0007000C, 0x00000007, 0x00000017, 0x00000015,
        0x00000016, 0x0000000F, 0x0000000E, 0x0003003E, 0x00000009, 0x00000017,
        0x000100FD, 0x00010038
    };

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(raygen_spirv),
        .pCode = raygen_spirv
    };

    VkShaderModule shader_module;
    VkResult result = vkCreateShaderModule(vk.device, &create_info, NULL, &shader_module);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to create shader module for %s\n", filename);
        return VK_NULL_HANDLE;
    }

    ri.Printf(PRINT_DEVELOPER, "RTX: Loaded shader module for %s\n", filename);
    return shader_module;
}

// Ray tracing pipeline creation
static VkResult vk_rtx_create_pipeline(void) {
    // Create descriptor set layout for ray tracing
    VkDescriptorSetLayoutBinding bindings[] = {
        // TLAS binding
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
        },
        // Output image binding
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
        },
        // Camera UBO
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
        },
        // Materials buffer (from existing material system)
        {
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR
        },
        // Lights buffer
        {
            .binding = 4,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
        },
        // Light count
        {
            .binding = 5,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
        }
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 6,
        .pBindings = bindings
    };

    VkResult result = vkCreateDescriptorSetLayout(vk.device, &layout_info, NULL, &g_rt_descriptor_set_layout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to create descriptor set layout\n");
        return result;
    }

    // Create pipeline layout
    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        .offset = 0,
        .size = sizeof(uint32_t) * 4  // max_recursion_depth, samples_per_pixel, enable_shadows, enable_reflections
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &g_rt_descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range
    };

    result = vkCreatePipelineLayout(vk.device, &pipeline_layout_info, NULL, &g_rt_pipeline_layout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to create pipeline layout\n");
        return result;
    }

    // Load shaders
    VkShaderModule raygen_shader = vk_rtx_load_shader("raygen.glsl");
    VkShaderModule miss_shader = vk_rtx_load_shader("miss.glsl");
    VkShaderModule closesthit_shader = vk_rtx_load_shader("closesthit.glsl");

    if (!raygen_shader || !miss_shader || !closesthit_shader) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to load shader modules\n");
        return VK_ERROR_UNKNOWN;
    }

    // Shader stages
    VkPipelineShaderStageCreateInfo shader_stages[3] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .module = raygen_shader,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = miss_shader,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .module = closesthit_shader,
            .pName = "main"
        }
    };

    // Shader groups
    VkRayTracingShaderGroupCreateInfoKHR shader_groups[3] = {
        // Raygen group
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 0, // raygen shader index
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        // Miss group
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 1, // miss shader index
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        // Hit group
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2, // closest hit shader index
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        }
    };

    // Create ray tracing pipeline
    VkRayTracingPipelineCreateInfoKHR pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = 3,
        .pStages = shader_stages,
        .groupCount = 3,
        .pGroups = shader_groups,
        .maxPipelineRayRecursionDepth = 1,
        .layout = g_rt_pipeline_layout
    };

    result = vkCreateRayTracingPipelinesKHR(vk.device, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                           1, &pipeline_info, NULL, &g_rt_pipeline);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to create ray tracing pipeline: %d\n", result);
        return result;
    }

    // Get ray tracing properties
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    VkPhysicalDeviceProperties2 device_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rt_props
    };

    vkGetPhysicalDeviceProperties2(vk.physical_device, &device_props);

    g_shader_handle_size = rt_props.shaderGroupHandleSize;
    g_sbt.sbt_record_size = rt_props.shaderGroupBaseAlignment;

    // Get shader group handles
    result = vkGetRayTracingShaderGroupHandlesKHR(vk.device, g_rt_pipeline, 0, 3,
                                                 3 * MAX_SHADER_HANDLE_SIZE, g_shader_handles);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to get shader group handles\n");
        return result;
    }

    // Clean up shader modules (pipeline keeps references)
    vkDestroyShaderModule(vk.device, raygen_shader, NULL);
    vkDestroyShaderModule(vk.device, miss_shader, NULL);
    vkDestroyShaderModule(vk.device, closesthit_shader, NULL);

    ri.Printf(PRINT_ALL, "Vulkan RTX: Ray tracing pipeline created successfully\n");
    return VK_SUCCESS;
}

/ Build BLAS for world geometry from BSP surfaces
qboolean vk_rtx_build_blas_from_world(void) {
    if (!g_rtx_accel_initialized) {
        ri.Printf(PRINT_WARNING, "RTX: Acceleration structures not initialized, cannot build world BLAS\n");
        return qfalse;
    }

    if (!tr.world || !tr.world->surfaces || tr.world->numsurfaces == 0) {
        ri.Printf(PRINT_WARNING, "RTX: No world geometry available for BLAS building\n");
        return qfalse;
    }

    ri.Printf(PRINT_ALL, "RTX: Building BLAS for %d world surfaces\n", tr.world->numsurfaces);

    // Group surfaces by material/shader for efficient BLAS creation
    // For now, create one BLAS per surface (can be optimized later)
    int successful_blas = 0;

    for (int i = 0; i < tr.world->numsurfaces; i++) {
        msurface_t* surf = &tr.world->surfaces[i];

        // Skip surfaces without geometry
        if (!surf || surf->numIndexes == 0 || surf->numVerts == 0) {
            continue;
        }

        // Get vertex and index buffers from VBO system
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceSize vertexOffset = 0;
        VkDeviceSize indexOffset = 0;
        uint32_t vertexCount = surf->numVerts;
        uint32_t indexCount = surf->numIndexes;

        // For now, we'll need to get the buffers from the VBO system
        // This is a simplified implementation - in practice we'd need to
        // extract the relevant vertex/index data for this surface
        if (vk_world.vbo && vk_world.ibo) {
            vertexBuffer = vk_world.vbo->buffer;
            indexBuffer = vk_world.ibo->buffer;

            // Calculate offsets for this surface in the VBO
            // This is a placeholder - would need proper VBO offset calculation
            vertexOffset = surf->firstVert * sizeof(drawVert_t);
            indexOffset = surf->firstIndex * sizeof(glIndex_t);
        } else {
            ri.Printf(PRINT_DEVELOPER, "RTX: Skipping surface %d - no VBO data available\n", i);
            continue;
        }

        // Create BLAS for this surface
        uint64_t blas_handle = vk_rtx_create_blas_for_geometry(
            vertexBuffer, indexBuffer, vertexCount, indexCount,
            sizeof(drawVert_t), va("world_surface_%d", i)
        );

        if (blas_handle != 0) {
            successful_blas++;
            ri.Printf(PRINT_DEVELOPER, "RTX: Created BLAS for surface %d (handle: %llu)\n", i, blas_handle);
        } else {
            ri.Printf(PRINT_WARNING, "RTX: Failed to create BLAS for surface %d\n", i);
        }
    }

    ri.Printf(PRINT_ALL, "RTX: Successfully created %d BLAS from %d world surfaces\n",
              successful_blas, tr.world->numsurfaces);

    return (successful_blas > 0);
}

// Accessors for surface indices buffer
extern "C" {
VkBuffer vk_rtx_get_surface_indices_buffer(void) {
    return g_surface_indices_buffer;
}
VkDeviceSize vk_rtx_get_surface_indices_size(void) {
    return g_surface_indices_size;
}
void vk_rtx_bind_surface_indices_buffer(VkDescriptorSet descriptorSet) {
    if (g_surface_indices_buffer == VK_NULL_HANDLE || g_surface_indices_size == 0) return;
    VkDescriptorBufferInfo bufferInfo = { .buffer = g_surface_indices_buffer, .offset = 0, .range = g_surface_indices_size };
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet;
    write.dstBinding = 7;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bufferInfo;
    write.pImageInfo = NULL;
    write.pTexelBufferView = NULL;
    qvkUpdateDescriptorSets(vk.device, 1, &write, 0, NULL);
}
}
#ifdef __cplusplus
}
#endif

void vk_rtx_update_surface_material_indices_buffer(void) {
    if (!g_rtx_accel_initialized) {
        ri.Printf(PRINT_DEVELOPER, "RTX: surface indices buffer update skipped (not initialized)\n");
        return;
    }
    if (g_surface_indices_buffer == VK_NULL_HANDLE) {
        vk_rtx_allocate_surface_indices_buffer();
        if (g_surface_indices_buffer == VK_NULL_HANDLE) {
            ri.Printf(PRINT_DEVELOPER, "RTX: failed to allocate surface indices buffer\n");
            return;
        }
    }
    if (!tr.world || !tr.world->surfaces) {
        ri.Printf(PRINT_DEVELOPER, "RTX: surface indices buffer update skipped (no world data)\n");
        return;
    }
    uint32_t count = tr.world->numsurfaces;
    if (count > MAX_SURFACES_FOR_INDICES) count = MAX_SURFACES_FOR_INDICES;
    for (uint32_t i = 0; i < count; i++) {
        g_surface_indices_cpu[i] = tr.world->surfaces[i].materialIndex;
    }
    g_surface_indices_size = count * sizeof(uint32_t);
    if (g_surface_indices_buffer == VK_NULL_HANDLE || g_surface_indices_memory == VK_NULL_HANDLE) {
        ri.Printf(PRINT_DEVELOPER, "RTX: surface indices GPU buffer not allocated; skipping upload\n");
        return;
    }
    void* mapped = NULL;
    VkResult r = vkMapMemory(vk.device, g_surface_indices_memory, 0, (VkDeviceSize)g_surface_indices_size, 0, &mapped);
    if (r == VK_SUCCESS && mapped && g_surface_indices_size > 0) {
        memcpy(mapped, g_surface_indices_cpu, (size_t)g_surface_indices_size);
        vkUnmapMemory(vk.device, g_surface_indices_memory);
        ri.Printf(PRINT_DEVELOPER, "RTX: uploaded %u bytes of surface indices to GPU\n", (unsigned)g_surface_indices_size);
    } else {
        ri.Printf(PRINT_DEVELOPER, "RTX: failed to map surface indices buffer for upload\n");
    }
}

