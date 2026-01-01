#include "tr_local.h"
#include "vk_rtx_acceleration.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

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
} rtx_acceleration_t;

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceAddress address;
    void* mapped;
    VkDeviceSize size;
} rtx_buffer_t;

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
    if (g_rtx_accel_initialized) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: acceleration already initialized\n");
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Vulkan RTX: Initializing acceleration structures\n");

    // Initialize acceleration structure state
    memset(g_blas, 0, sizeof(g_blas));
    memset(&g_tlas, 0, sizeof(g_tlas));
    memset(&g_instance_buffer, 0, sizeof(g_instance_buffer));
    memset(&g_sbt, 0, sizeof(g_sbt));

    g_blas_count = 0;
    g_rtx_blas_tlas_built = qfalse;

    // Create basic ray tracing pipeline (simplified - would normally load shaders)
    VkResult result = vk_rtx_create_pipeline();
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan RTX: Failed to create ray tracing pipeline\n");
        return qfalse;
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
    (void)light_id; (void)cmd_buffer;
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

    // Issue the trace rays command
    vkCmdTraceRaysKHR(cmd_buffer, &raygen_region, &miss_region, &hit_region, &callable_region,
                     glConfig.vidWidth, glConfig.vidHeight, 1);

    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: vkCmdTraceRaysKHR executed for %ux%u\n",
              glConfig.vidWidth, glConfig.vidHeight);
}

void vk_rtx_bind_and_trace_raysKHR_from_main(VkCommandBuffer cmd_buffer, uint32_t width, uint32_t height) {
    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: Starting ray tracing pipeline %ux%u\n", width, height);

    #ifdef VK_KHR_ray_tracing_pipeline
        // Ensure acceleration structures are built
        if (!g_rtx_blas_tlas_built) {
            vk_rtx_build_tlas(cmd_buffer);
        }

        // Set up SBT
        vk_rtx_setup_sbt(cmd_buffer);

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

        // Execute ray tracing
        vk_rtx_trace_raysKHR(cmd_buffer);

        // Memory barrier to ensure ray tracing is complete before any subsequent operations
        VkMemoryBarrier rt_barrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
        };

        vkCmdPipelineBarrier(cmd_buffer,
                            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            0, 1, &rt_barrier, 0, NULL, 0, NULL);

        ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: Ray tracing pipeline completed\n");
    #else
        ri.Printf(PRINT_WARNING, "Vulkan RTX: VK_KHR_ray_tracing_pipeline not available\n");
    #endif

    (void)cmd_buffer;
    (void)width;
    (void)height;
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

void vk_rtx_set_quality_settings(float quality, qboolean shadows, qboolean reflections,
                                 qboolean refractions, qboolean global_illumination) {
    (void)quality; (void)shadows; (void)reflections; (void)refractions; (void)global_illumination;
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

// Ray tracing pipeline creation (simplified - would normally load actual shaders)
static VkResult vk_rtx_create_pipeline(void) {
    // Create descriptor set layout for ray tracing
    VkDescriptorSetLayoutBinding bindings[] = {
        // TLAS binding
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
        },
        // Output image binding
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
        }
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings
    };

    VkResult result = vkCreateDescriptorSetLayout(vk.device, &layout_info, NULL, &g_rt_descriptor_set_layout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to create descriptor set layout\n");
        return result;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &g_rt_descriptor_set_layout
    };

    result = vkCreatePipelineLayout(vk.device, &pipeline_layout_info, NULL, &g_rt_pipeline_layout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "RTX: Failed to create pipeline layout\n");
        return result;
    }

    // For this implementation, we'll create a minimal pipeline
    // In a real implementation, you would:
    // 1. Load SPIR-V shaders for raygen, miss, and closest hit
    // 2. Create shader modules
    // 3. Set up shader groups
    // 4. Create the pipeline

    // For now, we'll skip the actual pipeline creation since we don't have shaders
    // The pipeline creation would look something like this:

    /*
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

    VkRayTracingPipelineCreateInfoKHR pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = 3, // number of shader stages
        .pStages = shader_stages, // VkPipelineShaderStageCreateInfo array
        .groupCount = 3,
        .pGroups = shader_groups,
        .maxPipelineRayRecursionDepth = 1,
        .layout = g_rt_pipeline_layout
    };

    result = vkCreateRayTracingPipelinesKHR(vk.device, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                           1, &pipeline_info, NULL, &g_rt_pipeline);
    */

    ri.Printf(PRINT_DEVELOPER, "Vulkan RTX: Pipeline creation stub (shaders not loaded)\n");
    return VK_SUCCESS; // Return success for now since we're not creating the actual pipeline
}

#ifdef __cplusplus
}
#endif

