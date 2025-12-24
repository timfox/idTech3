#include "vk_utils.h"
#include "../renderercommon/tr_public.h"
#include "vk.h"
#include <math.h>

// Renderer interface
extern refimport_t ri;

// Memory tracking
vk_memory_stats_t vk_memory_stats = {0};
vk_vram_stats_t vk_vram_stats = {0};
vk_memory_tracker_t vk_memory_tracker = {0};

// Memory tracking functions
extern void vk_track_allocation(VkDeviceSize size);
extern void vk_track_free(VkDeviceSize size);

// Runtime safety checks for Vulkan compatibility
void vk_safety_checks(void) {
    // Ensure VK_NULL_HANDLE compatibility
    if (VK_NULL_HANDLE != NULL) {
        ri.Printf(PRINT_WARNING, "VK_NULL_HANDLE != NULL - this may cause compatibility issues\n");
    }
}

// Modern bounds checking for array operations
qboolean vk_bounds_check(size_t index, size_t max, const char *array_name) {
    if (index >= max) {
        ri.Printf(PRINT_ERROR, "Vulkan: Array index %zu out of bounds for %s (max %zu)\n", index, array_name, max);
        return qfalse;
    }
    return qtrue;
}

// Safe Vulkan handle validation
qboolean vk_validate_handle(void *handle, const char *handle_name) {
    if (handle == VK_NULL_HANDLE || handle == NULL) {
        ri.Printf(PRINT_ERROR, "Vulkan: Invalid %s handle (NULL)\n", handle_name);
        return qfalse;
    }
    return qtrue;
}

// Modern Vulkan performance monitoring
void vk_performance_marker_begin(VkCommandBuffer cmd, const char *name) {
#ifdef USE_VK_VALIDATION
    if (qvkCmdBeginDebugUtilsLabelEXT && name && cmd != VK_NULL_HANDLE) {
        VkDebugUtilsLabelEXT label = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pLabelName = name,
            .color = {0.0f, 1.0f, 0.0f, 1.0f} // Green for performance markers
        };
        qvkCmdBeginDebugUtilsLabelEXT(cmd, &label);
    }
#else
    (void)cmd; // Suppress unused parameter warning
    (void)name;
#endif
}

void vk_performance_marker_end(VkCommandBuffer cmd) {
#ifdef USE_VK_VALIDATION
    if (qvkCmdEndDebugUtilsLabelEXT && cmd != VK_NULL_HANDLE) {
        qvkCmdEndDebugUtilsLabelEXT(cmd);
    }
#else
    (void)cmd; // Suppress unused parameter warning
#endif
}

// Utility function to check for NaN/Inf in floating point values
qboolean vk_validate_float(float value) {
    if (isnan(value) || isinf(value)) {
        ri.Printf(PRINT_WARNING, "Vulkan: Invalid float value detected: %f\n", value);
        return qfalse;
    }
    return qtrue;
}

qboolean vk_validate_vec3(vec3_t v) {
    return vk_validate_float(v[0]) && vk_validate_float(v[1]) && vk_validate_float(v[2]);
}

qboolean vk_validate_vec4(vec4_t v) {
    return vk_validate_float(v[0]) && vk_validate_float(v[1]) && vk_validate_float(v[2]) && vk_validate_float(v[3]);
}

// Sanitize floating point values to prevent NaN/Inf propagation
float vk_sanitize_float(float value, float default_value) {
    if (isnan(value) || isinf(value)) {
        ri.Printf(PRINT_WARNING, "Vulkan: Sanitized invalid float %f to %f\n", value, default_value);
        return default_value;
    }
    return value;
}

void vk_sanitize_vec3(vec3_t v, float default_value) {
    for (int i = 0; i < 3; i++) {
        v[i] = vk_sanitize_float(v[i], default_value);
    }
}

void vk_sanitize_vec4(vec4_t v, float default_value) {
    for (int i = 0; i < 4; i++) {
        v[i] = vk_sanitize_float(v[i], default_value);
    }
}

// Validate shader inputs before passing to Vulkan
qboolean vk_validate_shader_inputs(const Vk_Pipeline_Def *def) {
    if (def == NULL) {
        ri.Printf(PRINT_ERROR, "vk_validate_shader_inputs: def is NULL\n");
        return qfalse;
    }

    // TODO: Add shader input validation when needed
    // For now, just basic null checks

    return qtrue;
}

// Initialize VRAM statistics with device properties
void vk_init_vram_stats(void) {
    if (vk.physical_device == VK_NULL_HANDLE) {
        return;
    }

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(vk.physical_device, &mem_props);

    vk.vram_stats.total_vram = 0;
    for (uint32_t i = 0; i < mem_props.memoryHeapCount; i++) {
        if (mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            vk.vram_stats.total_vram += mem_props.memoryHeaps[i].size;
        }
    }

    vk.vram_stats.available_vram = vk.vram_stats.total_vram;
    atomic_init(&vk.vram_stats.total_allocations, 0);
    atomic_init(&vk.vram_stats.current_allocations, 0);
    atomic_init(&vk.vram_stats.freed_allocations, 0);
    atomic_init(&vk.vram_stats.leaked_allocations, 0);
    atomic_init(&vk.vram_stats.memory_leaks_detected, qfalse);
    vk_memory_tracker.leak_detection_enabled = qtrue;

    ri.Printf(PRINT_ALL, "VRAM initialized: %lu MB total\n",
        (unsigned long)(vk.vram_stats.total_vram / (1024 * 1024)));
}

// Track GPU memory allocation with detailed information
void vk_track_gpu_allocation(VkDeviceMemory memory, VkDeviceSize size, uint32_t memory_type,
                            const char *resource_name, const char *allocation_site) {
    if (!vk_memory_tracker.leak_detection_enabled) {
        return;
    }

    if (vk_memory_tracker.allocation_count >= VK_MAX_MEMORY_ALLOCATIONS) {
        ri.Printf(PRINT_WARNING, "GPU memory tracker full, cannot track allocation\n");
        return;
    }

    uint32_t index = atomic_fetch_add_explicit(&vk_memory_tracker.allocation_count, 1, memory_order_relaxed);
    vk_memory_allocation_t *alloc = &vk_memory_tracker.allocations[index];

    alloc->memory = memory;
    alloc->size = size;
    alloc->memory_type = memory_type;
    alloc->resource_name = resource_name;
    alloc->allocation_site = allocation_site;
    alloc->allocation_id = atomic_fetch_add_explicit(&vk_memory_tracker.next_allocation_id, 1, memory_order_relaxed);
    alloc->is_freed = qfalse;

    // Update VRAM statistics
    vk.vram_stats.used_vram += size;
    vk.vram_stats.available_vram -= size;
    atomic_fetch_add_explicit(&vk.vram_stats.total_allocations, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&vk.vram_stats.current_allocations, 1, memory_order_relaxed);
    vk.vram_stats.memory_type_usage[memory_type] += size;

    if (vk.vram_stats.used_vram > vk.vram_stats.max_used_vram) {
        vk.vram_stats.max_used_vram = vk.vram_stats.used_vram;
    }

    // Basic leak detection warning
    if (atomic_load_explicit(&vk.vram_stats.current_allocations, memory_order_relaxed) > 1000) {
        ri.Printf(PRINT_WARNING, "High number of GPU allocations: %u\n", atomic_load_explicit(&vk.vram_stats.current_allocations, memory_order_relaxed));
    }
}

// Track GPU memory deallocation
void vk_track_gpu_free(VkDeviceMemory memory) {
    if (!vk_memory_tracker.leak_detection_enabled) {
        return;
    }

    for (uint32_t i = 0; i < vk_memory_tracker.allocation_count; i++) {
        vk_memory_allocation_t *alloc = &vk_memory_tracker.allocations[i];
        if (alloc->memory == memory && !alloc->is_freed) {
            alloc->is_freed = qtrue;

            // Update VRAM statistics
            vk.vram_stats.used_vram -= alloc->size;
            vk.vram_stats.available_vram += alloc->size;
            atomic_fetch_add_explicit(&vk.vram_stats.freed_allocations, 1, memory_order_relaxed);
            atomic_fetch_sub_explicit(&vk.vram_stats.current_allocations, 1, memory_order_relaxed);
            vk.vram_stats.memory_type_usage[alloc->memory_type] -= alloc->size;

            return;
        }
    }

    ri.Printf(PRINT_WARNING, "GPU memory free: allocation not found in tracker\n");
}

// Detect memory leaks and report them
void vk_detect_memory_leaks(void) {
    if (!vk_memory_tracker.leak_detection_enabled) {
        return;
    }

    uint32_t leak_count = 0;
    ri.Printf(PRINT_ALL, "=== GPU Memory Leak Detection ===\n");

    for (uint32_t i = 0; i < vk_memory_tracker.allocation_count; i++) {
        vk_memory_allocation_t *alloc = &vk_memory_tracker.allocations[i];
        if (!alloc->is_freed) {
            ri.Printf(PRINT_WARNING, "LEAK: Allocation ID %u, %s (%lu bytes) at %s\n",
                alloc->allocation_id,
                alloc->resource_name ? alloc->resource_name : "unnamed",
                (unsigned long)alloc->size,
                alloc->allocation_site ? alloc->allocation_site : "unknown");
            leak_count++;
        }
    }

    if (leak_count == 0) {
        ri.Printf(PRINT_ALL, "No GPU memory leaks detected\n");
    } else {
        ri.Printf(PRINT_ERROR, "Found %u GPU memory leaks\n", leak_count);
        atomic_store_explicit(&vk.vram_stats.leaked_allocations, leak_count, memory_order_relaxed);
        atomic_store_explicit(&vk.vram_stats.memory_leaks_detected, qtrue, memory_order_relaxed);
    }
}

// Print comprehensive VRAM usage statistics
void vk_print_vram_stats(void) {
    ri.Printf(PRINT_ALL, "=== Vulkan VRAM Statistics ===\n");
    ri.Printf(PRINT_ALL, "Total VRAM: %lu MB\n", (unsigned long)(vk.vram_stats.total_vram / (1024 * 1024)));
    ri.Printf(PRINT_ALL, "Used VRAM: %lu MB (%.1f%%)\n",
        (unsigned long)(vk.vram_stats.used_vram / (1024 * 1024)),
        vk.vram_stats.total_vram > 0 ? (float)vk.vram_stats.used_vram / vk.vram_stats.total_vram * 100.0f : 0.0f);
    ri.Printf(PRINT_ALL, "Available VRAM: %lu MB\n", (unsigned long)(vk.vram_stats.available_vram / (1024 * 1024)));
    ri.Printf(PRINT_ALL, "Peak Usage: %lu MB\n", (unsigned long)(vk.vram_stats.max_used_vram / (1024 * 1024)));

    ri.Printf(PRINT_ALL, "Allocation Stats:\n");
    ri.Printf(PRINT_ALL, "  Total allocations: %u\n", atomic_load_explicit(&vk.vram_stats.total_allocations, memory_order_relaxed));
    ri.Printf(PRINT_ALL, "  Current allocations: %u\n", atomic_load_explicit(&vk.vram_stats.current_allocations, memory_order_relaxed));
    ri.Printf(PRINT_ALL, "  Freed allocations: %u\n", atomic_load_explicit(&vk.vram_stats.freed_allocations, memory_order_relaxed));
    ri.Printf(PRINT_ALL, "  Memory leaks: %u\n", atomic_load_explicit(&vk.vram_stats.leaked_allocations, memory_order_relaxed));

    // Print memory type usage
    VkPhysicalDeviceMemoryProperties mem_props;
    if (vk.physical_device != VK_NULL_HANDLE) {
        vkGetPhysicalDeviceMemoryProperties(vk.physical_device, &mem_props);
        ri.Printf(PRINT_ALL, "Memory Type Usage:\n");
        for (uint32_t i = 0; i < mem_props.memoryTypeCount && i < VK_MAX_MEMORY_TYPES; i++) {
            if (vk.vram_stats.memory_type_usage[i] > 0) {
                const char *heap_type = (mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? "GPU" : "CPU";
                ri.Printf(PRINT_ALL, "  Type %u (%s): %lu MB\n", i, heap_type,
                    (unsigned long)(vk.vram_stats.memory_type_usage[i] / (1024 * 1024)));
            }
        }
    }
}

// Print memory statistics for leak detection
void vk_print_memory_stats(void) {
    ri.Printf(PRINT_ALL, "=== Vulkan Memory Statistics ===\n");
    ri.Printf(PRINT_ALL, "  Allocations: %u\n", atomic_load_explicit(&vk_memory_stats.allocations, memory_order_relaxed));
    ri.Printf(PRINT_ALL, "  Frees: %u\n", atomic_load_explicit(&vk_memory_stats.frees, memory_order_relaxed));
    ri.Printf(PRINT_ALL, "  Current: %u\n", atomic_load_explicit(&vk_memory_stats.current_allocations, memory_order_relaxed));
    ri.Printf(PRINT_ALL, "  Total allocated: %lu bytes\n", (unsigned long)vk_memory_stats.total_allocated_bytes);
    ri.Printf(PRINT_ALL, "  Total freed: %lu bytes\n", (unsigned long)vk_memory_stats.total_freed_bytes);

    uint32_t current_allocs = atomic_load_explicit(&vk_memory_stats.current_allocations, memory_order_relaxed);
    if (current_allocs > 0) {
        ri.Printf(PRINT_WARNING, "  Potential memory leak: %u unfreed allocations\n", current_allocs);
    }

    // Print VRAM stats too
    vk_print_vram_stats();

    // Run leak detection
    vk_detect_memory_leaks();
}
