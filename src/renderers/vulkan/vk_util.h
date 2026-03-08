/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan renderer utility helpers: parsing, matrix math, color normalization.
===========================================================================
*/

#pragma once

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parse "r g b" string into vec3. Returns qfalse on parse failure. */
qboolean vk_parse_rgb_string( const char *s, vec3_t out );

/* Maximum absolute difference between two 4x4 matrices (float[16]). */
float vk_matrix_max_abs_diff( const float *a, const float *b );

/* Normalize RGB by max component; if zero, set to (1,1,1). */
void vk_normalize_rgb_luma_safe( vec3_t io );

#ifdef __cplusplus
}
#endif
