#include "vk_utils.h"
#include "../renderercommon/tr_public.h"
#include "vk.h"
#include <math.h>

// Renderer interface
extern refimport_t ri;

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

// Print memory statistics for leak detection
void vk_print_memory_stats(void) {
    ri.Printf(PRINT_ALL, "Vulkan Memory Stats:\n");
    ri.Printf(PRINT_ALL, "  Allocations: %u\n", vk_memory_stats.allocations);
    ri.Printf(PRINT_ALL, "  Frees: %u\n", vk_memory_stats.frees);
    ri.Printf(PRINT_ALL, "  Current: %u\n", vk_memory_stats.current_allocations);
    ri.Printf(PRINT_ALL, "  Total allocated: %lu bytes\n", (unsigned long)vk_memory_stats.total_allocated_bytes);
    ri.Printf(PRINT_ALL, "  Total freed: %lu bytes\n", (unsigned long)vk_memory_stats.total_freed_bytes);

    if (vk_memory_stats.current_allocations > 0) {
        ri.Printf(PRINT_WARNING, "  Potential memory leak: %u unfreed allocations\n", vk_memory_stats.current_allocations);
    }
}
