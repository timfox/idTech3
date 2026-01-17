/*
===============================================================================
SIMD-Optimized Math Operations for id Tech 3

This file provides SIMD-accelerated versions of common vector and matrix
operations for improved performance on supported architectures.
===============================================================================
*/

#include "q_shared.h"
#include <math.h>

// SIMD optimization detection and includes
#if defined(__SSE__) || defined(__SSE2__) || defined(__SSE3__) || defined(__SSSE3__) || defined(__SSE4_1__) || defined(__SSE4_2__)
#define Q_MATH_USE_SSE 1
#include <xmmintrin.h>
#include <emmintrin.h>
#ifdef __SSE4_1__
#include <smmintrin.h>
#endif
#endif

#if defined(__ARM_NEON__) || defined(__ARM_NEON)
#define Q_MATH_USE_NEON 1
#include <arm_neon.h>
#endif

// Feature detection
static qboolean q_math_simd_available = qfalse;
static int q_math_simd_features = 0;

#define SIMD_FEATURE_SSE    (1 << 0)
#define SIMD_FEATURE_SSE2   (1 << 1)
#define SIMD_FEATURE_SSE3   (1 << 2)
#define SIMD_FEATURE_SSSE3  (1 << 3)
#define SIMD_FEATURE_SSE41  (1 << 4)
#define SIMD_FEATURE_NEON   (1 << 5)

// Initialize SIMD math system
void Math_SIMD_Init(void) {
    q_math_simd_available = qfalse;
    q_math_simd_features = 0;

#if defined(Q_MATH_USE_SSE)
    q_math_simd_available = qtrue;
    q_math_simd_features |= SIMD_FEATURE_SSE;

    // Check for SSE2
    #if defined(__SSE2__)
    q_math_simd_features |= SIMD_FEATURE_SSE2;
    #endif

    // Check for SSE3
    #if defined(__SSE3__)
    q_math_simd_features |= SIMD_FEATURE_SSE3;
    #endif

    // Check for SSSE3
    #if defined(__SSSE3__)
    q_math_simd_features |= SIMD_FEATURE_SSSE3;
    #endif

    // Check for SSE4.1
    #if defined(__SSE4_1__)
    q_math_simd_features |= SIMD_FEATURE_SSE41;
    #endif

    Com_Printf("Math SIMD: SSE optimizations enabled\n");

#elif defined(Q_MATH_USE_NEON)
    q_math_simd_available = qtrue;
    q_math_simd_features |= SIMD_FEATURE_NEON;
    Com_Printf("Math SIMD: NEON optimizations enabled\n");

#else
    Com_Printf("Math SIMD: No SIMD optimizations available\n");
#endif
}

// SIMD-accelerated vector operations
#if defined(Q_MATH_USE_SSE)

static inline __m128 Vec3ToM128(const vec3_t v) {
    return _mm_setr_ps(v[0], v[1], v[2], 0.0f);
}

static inline void M128ToVec3(__m128 m, vec3_t v) {
    _mm_storeu_ps(v, m);
}

void VectorAdd_SIMD(const vec3_t a, const vec3_t b, vec3_t out) {
    __m128 va = Vec3ToM128(a);
    __m128 vb = Vec3ToM128(b);
    __m128 result = _mm_add_ps(va, vb);
    M128ToVec3(result, out);
}

void VectorSubtract_SIMD(const vec3_t a, const vec3_t b, vec3_t out) {
    __m128 va = Vec3ToM128(a);
    __m128 vb = Vec3ToM128(b);
    __m128 result = _mm_sub_ps(va, vb);
    M128ToVec3(result, out);
}

void VectorScale_SIMD(const vec3_t v, float scale, vec3_t out) {
    __m128 vv = Vec3ToM128(v);
    __m128 vs = _mm_set1_ps(scale);
    __m128 result = _mm_mul_ps(vv, vs);
    M128ToVec3(result, out);
}

float VectorDot_SIMD(const vec3_t a, const vec3_t b) {
    __m128 va = Vec3ToM128(a);
    __m128 vb = Vec3ToM128(b);
    __m128 product = _mm_mul_ps(va, vb);

    // Horizontal add
    __m128 sum = _mm_add_ps(product, _mm_movehl_ps(product, product));
    sum = _mm_add_ss(sum, _mm_shuffle_ps(sum, sum, 1));

    return _mm_cvtss_f32(sum);
}

void VectorCross_SIMD(const vec3_t a, const vec3_t b, vec3_t out) {
    // a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x
    __m128 va = Vec3ToM128(a);
    __m128 vb = Vec3ToM128(b);

    __m128 tmp1 = _mm_shuffle_ps(va, va, _MM_SHUFFLE(3, 0, 2, 1)); // a: w,x,z,y
    __m128 tmp2 = _mm_shuffle_ps(vb, vb, _MM_SHUFFLE(3, 1, 0, 2)); // b: w,y,x,z
    __m128 result1 = _mm_mul_ps(tmp1, tmp2);

    tmp1 = _mm_shuffle_ps(va, va, _MM_SHUFFLE(3, 1, 0, 2)); // a: w,y,x,z
    tmp2 = _mm_shuffle_ps(vb, vb, _MM_SHUFFLE(3, 0, 2, 1)); // b: w,x,z,y
    __m128 result2 = _mm_mul_ps(tmp1, tmp2);

    __m128 result = _mm_sub_ps(result1, result2);
    M128ToVec3(result, out);
}

void VectorNormalize_SIMD(vec3_t v) {
    float length = VectorDot_SIMD(v, v);

    if (length > 0.0f) {
        length = 1.0f / sqrtf(length);
        VectorScale_SIMD(v, length, v);
    }
}

void MatrixMultiply_SIMD(const float a[16], const float b[16], float out[16]) {
    // 4x4 matrix multiplication using SSE
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            __m128 row = _mm_set1_ps(a[i * 4 + j]);
            __m128 col = _mm_loadu_ps(&b[j * 4]);
            __m128 product = _mm_mul_ps(row, col);

            if (j == 0) {
                _mm_storeu_ps(&out[i * 4], product);
            } else {
                __m128 sum = _mm_loadu_ps(&out[i * 4]);
                sum = _mm_add_ps(sum, product);
                _mm_storeu_ps(&out[i * 4], sum);
            }
        }
    }
}

#elif defined(Q_MATH_USE_NEON)

void VectorAdd_SIMD(const vec3_t a, const vec3_t b, vec3_t out) {
    float32x4_t va = vld1q_f32(a);
    float32x4_t vb = vld1q_f32(b);
    float32x4_t result = vaddq_f32(va, vb);
    vst1q_f32(out, result);
}

void VectorSubtract_SIMD(const vec3_t a, const vec3_t b, vec3_t out) {
    float32x4_t va = vld1q_f32(a);
    float32x4_t vb = vld1q_f32(b);
    float32x4_t result = vsubq_f32(va, vb);
    vst1q_f32(out, result);
}

void VectorScale_SIMD(const vec3_t v, float scale, vec3_t out) {
    float32x4_t vv = vld1q_f32(v);
    float32x4_t vs = vdupq_n_f32(scale);
    float32x4_t result = vmulq_f32(vv, vs);
    vst1q_f32(out, result);
}

float VectorDot_SIMD(const vec3_t a, const vec3_t b) {
    float32x4_t va = vld1q_f32(a);
    float32x4_t vb = vld1q_f32(b);
    float32x4_t product = vmulq_f32(va, vb);

    // Horizontal add
    float32x2_t sum = vadd_f32(vget_low_f32(product), vget_high_f32(product));
    return vget_lane_f32(vpadd_f32(sum, sum), 0);
}

void VectorCross_SIMD(const vec3_t a, const vec3_t b, vec3_t out) {
    float32x4_t va = vld1q_f32(a);
    float32x4_t vb = vld1q_f32(b);

    // Shuffle for cross product calculation
    float32x4_t a_yzx = vshuffle_f32(va, _MM_SHUFFLE(3, 0, 2, 1));
    float32x4_t b_zxy = vshuffle_f32(vb, _MM_SHUFFLE(3, 1, 0, 2));
    float32x4_t a_zxy = vshuffle_f32(va, _MM_SHUFFLE(3, 1, 0, 2));
    float32x4_t b_yzx = vshuffle_f32(vb, _MM_SHUFFLE(3, 0, 2, 1));

    float32x4_t result = vsubq_f32(vmulq_f32(a_yzx, b_zxy), vmulq_f32(a_zxy, b_yzx));
    vst1q_f32(out, result);
}

void VectorNormalize_SIMD(vec3_t v) {
    float length = VectorDot_SIMD(v, v);

    if (length > 0.0f) {
        length = 1.0f / sqrtf(length);
        VectorScale_SIMD(v, length, v);
    }
}

void MatrixMultiply_SIMD(const float a[16], const float b[16], float out[16]) {
    // NEON-optimized 4x4 matrix multiplication
    for (int i = 0; i < 4; i++) {
        float32x4_t row = vld1q_f32(&a[i * 4]);
        vst1q_f32(&out[i * 4], vmulq_n_f32(row, b[i * 4 + 0]));

        for (int j = 1; j < 4; j++) {
            float32x4_t col = vld1q_f32(&b[j * 4]);
            float32x4_t product = vmulq_n_f32(col, a[i * 4 + j]);
            float32x4_t sum = vld1q_f32(&out[i * 4]);
            vst1q_f32(&out[i * 4], vaddq_f32(sum, product));
        }
    }
}

#else

// Fallback implementations for systems without SIMD
void VectorAdd_SIMD(const vec3_t a, const vec3_t b, vec3_t out) {
    VectorAdd(a, b, out);
}

void VectorSubtract_SIMD(const vec3_t a, const vec3_t b, vec3_t out) {
    VectorSubtract(a, b, out);
}

void VectorScale_SIMD(const vec3_t v, float scale, vec3_t out) {
    VectorScale(v, scale, out);
}

float VectorDot_SIMD(const vec3_t a, const vec3_t b) {
    return DotProduct(a, b);
}

void VectorCross_SIMD(const vec3_t a, const vec3_t b, vec3_t out) {
    CrossProduct(a, b, out);
}

void VectorNormalize_SIMD(vec3_t v) {
    VectorNormalize(v);
}

void MatrixMultiply_SIMD(const float a[16], const float b[16], float out[16]) {
    // Fallback matrix multiplication
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            out[i * 4 + j] = 0;
            for (int k = 0; k < 4; k++) {
                out[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
            }
        }
    }
}

#endif

// Performance comparison functions
typedef struct {
    const char *name;
    void (*func)(void);
    double time_taken;
} perf_test_t;

static double GetTime(void) {
    return (double)clock() / CLOCKS_PER_SEC * 1000.0;
}

void Math_SIMD_PerformanceTest(void) {
    const int ITERATIONS = 100000;
    vec3_t a = {1.0f, 2.0f, 3.0f};
    vec3_t b = {4.0f, 5.0f, 6.0f};
    vec3_t result;

    Com_Printf("Math SIMD Performance Test (%d iterations):\n", ITERATIONS);

    // Test VectorAdd
    double start_time = GetTime();
    for (int i = 0; i < ITERATIONS; i++) {
        VectorAdd(a, b, result);
    }
    double scalar_time = GetTime() - start_time;

    start_time = GetTime();
    for (int i = 0; i < ITERATIONS; i++) {
        VectorAdd_SIMD(a, b, result);
    }
    double simd_time = GetTime() - start_time;

    Com_Printf("VectorAdd: Scalar=%.2fms, SIMD=%.2fms (%.1fx speedup)\n",
               scalar_time, simd_time, scalar_time / (simd_time + 0.001));

    // Test VectorDot
    start_time = GetTime();
    for (int i = 0; i < ITERATIONS; i++) {
        volatile float dot = DotProduct(a, b);
        (void)dot;
    }
    scalar_time = GetTime() - start_time;

    start_time = GetTime();
    for (int i = 0; i < ITERATIONS; i++) {
        volatile float dot = VectorDot_SIMD(a, b);
        (void)dot;
    }
    simd_time = GetTime() - start_time;

    Com_Printf("VectorDot: Scalar=%.2fms, SIMD=%.2fms (%.1fx speedup)\n",
               scalar_time, simd_time, scalar_time / (simd_time + 0.001));
}

// Query SIMD capabilities
qboolean Math_SIMD_IsAvailable(void) {
    return q_math_simd_available;
}

int Math_SIMD_GetFeatures(void) {
    return q_math_simd_features;
}

const char *Math_SIMD_GetFeatureName(int feature) {
    switch (feature) {
        case SIMD_FEATURE_SSE: return "SSE";
        case SIMD_FEATURE_SSE2: return "SSE2";
        case SIMD_FEATURE_SSE3: return "SSE3";
        case SIMD_FEATURE_SSSE3: return "SSSE3";
        case SIMD_FEATURE_SSE41: return "SSE4.1";
        case SIMD_FEATURE_NEON: return "NEON";
        default: return "Unknown";
    }
}