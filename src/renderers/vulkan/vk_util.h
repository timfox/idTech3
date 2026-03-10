/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan renderer utility helpers: parsing, matrix math, color normalization.
===========================================================================
*/

#pragma once

#include "../common/tr_types.h"
#include "../common/vulkan/vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

/* VkPresentModeKHR to string for logging. */
const char *vk_present_mode_string( VkPresentModeKHR mode );

/* VkResult to string for error reporting. */
const char *vk_result_string( VkResult code );

/* VkFormat to string for logging. */
const char *vk_format_string( VkFormat format );

/* Find memory type index for allocation. Aborts on failure. */
uint32_t vk_find_memory_type( VkPhysicalDevice physical_device, uint32_t memory_type_bits, VkMemoryPropertyFlags properties );

/* Same as vk_find_memory_type but returns ~0U on failure (no abort). If outprops is non-NULL, writes the found type's property flags. */
uint32_t vk_find_memory_type2( VkPhysicalDevice physical_device, uint32_t memory_type_bits, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags *outprops );

/* Parse "r g b" string into vec3. Returns qfalse on parse failure. */
qboolean vk_parse_rgb_string( const char *s, vec3_t out );

/*
 * Parse fog tint string into vec3.
 * Accepts "r g b" and "r,g,b"; values above 1.5 are treated as 0..255 and normalized.
 * Returns qfalse for invalid/disabled (near-zero) tint.
 */
qboolean vk_parse_fog_tint_string( const char *s, vec3_t out );

/* Maximum absolute difference between two 4x4 matrices (float[16]). */
float vk_matrix_max_abs_diff( const float *a, const float *b );

/* Normalize RGB by max component; if zero, set to (1,1,1). */
void vk_normalize_rgb_luma_safe( vec3_t io );

#ifdef __cplusplus
}
#endif
