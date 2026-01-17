/*
===============================================================================
SIMD Math Operations Header

Function declarations for SIMD-accelerated math operations.
===============================================================================
*/

#ifndef __Q_MATH_SIMD_H__
#define __Q_MATH_SIMD_H__

#include "q_shared.h"

// SIMD detection and feature flags
extern qboolean q_math_simd_available;
extern int q_math_simd_features;

#define SIMD_FEATURE_SSE    (1 << 0)
#define SIMD_FEATURE_SSE2   (1 << 1)
#define SIMD_FEATURE_SSE3   (1 << 2)
#define SIMD_FEATURE_SSSE3  (1 << 3)
#define SIMD_FEATURE_SSE41  (1 << 4)
#define SIMD_FEATURE_NEON   (1 << 5)

// SIMD math functions
void Math_SIMD_Init(void);
qboolean Math_SIMD_IsAvailable(void);
int Math_SIMD_GetFeatures(void);
const char *Math_SIMD_GetFeatureName(int feature);

// SIMD-accelerated vector operations
void VectorAdd_SIMD(const vec3_t a, const vec3_t b, vec3_t out);
void VectorSubtract_SIMD(const vec3_t a, const vec3_t b, vec3_t out);
void VectorScale_SIMD(const vec3_t v, float scale, vec3_t out);
float VectorDot_SIMD(const vec3_t a, const vec3_t b);
void VectorCross_SIMD(const vec3_t a, const vec3_t b, vec3_t out);
void VectorNormalize_SIMD(vec3_t v);

// SIMD matrix operations
void MatrixMultiply_SIMD(const float a[16], const float b[16], float out[16]);

// Performance testing
void Math_SIMD_PerformanceTest(void);

#endif // __Q_MATH_SIMD_H__