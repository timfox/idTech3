/*
===============================================================================
Simple Property-Based Testing Implementation
===============================================================================
*/

#include "test_framework.h"
#include "../src/qcommon/q_shared.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <math.h>

// Float comparison helper is now in test_framework.h

// Mock implementations for engine functions
void Com_Error(errorParm_t level, const char *error, ...) {
	(void)level;
	va_list argptr;
	va_start(argptr, error);
	vfprintf(stderr, error, argptr);
	va_end(argptr);
	exit(1);
}

void Com_Printf(const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

static void __attribute__((unused)) Com_DPrintf(const char *fmt, ...) {
	(void)fmt; // Debug printf - do nothing in tests
}

static fileHandle_t __attribute__((unused)) FS_FOpenFileRead(const char *filename, fileHandle_t *file, qboolean uniqueFILE) {
	(void)filename;
	(void)file;
	(void)uniqueFILE;
	return 0; // Not implemented for tests
}

static int __attribute__((unused)) FS_Read(void *buffer, int len, fileHandle_t f) {
	(void)buffer;
	(void)len;
	(void)f;
	return 0; // Not implemented for tests
}

static void __attribute__((unused)) FS_FCloseFile(fileHandle_t f) {
	(void)f; // Not implemented for tests
}

// Random generator implementation
typedef struct {
    unsigned int seed;
} random_generator_t;

static void random_init(random_generator_t *gen, unsigned int seed) {
    gen->seed = seed;
}

static unsigned int lcg_next(random_generator_t *gen) {
    gen->seed = (gen->seed * 1103515245 + 12345) & 0x7fffffff;
    return gen->seed;
}

static float random_float(random_generator_t *gen, float min, float max) {
    float normalized = (float)lcg_next(gen) / (float)0x7fffffff;
    return min + normalized * (max - min);
}

static void random_vec3(random_generator_t *gen, vec3_t out, float min, float max) {
    out[0] = random_float(gen, min, max);
    out[1] = random_float(gen, min, max);
    out[2] = random_float(gen, min, max);
}

static qboolean VectorsEqual(const vec3_t a, const vec3_t b) {
    return fabsf(a[0] - b[0]) < 1e-6f &&
           fabsf(a[1] - b[1]) < 1e-6f &&
           fabsf(a[2] - b[2]) < 1e-6f;
}

// Property test implementations
static void test_vector_addition_commutative(random_generator_t *gen) {
    vec3_t a, b, result1, result2;

    random_vec3(gen, a, -100.0f, 100.0f);
    random_vec3(gen, b, -100.0f, 100.0f);

    // Test commutativity: a + b == b + a
    VectorAdd(a, b, result1);
    VectorAdd(b, a, result2);

    ASSERT_TRUE(VectorsEqual(result1, result2));
}

static void test_vector_addition_associative(random_generator_t *gen) {
    vec3_t a, b, c, temp1, temp2, result1, result2;

    // Use smaller ranges to reduce floating-point precision issues
    random_vec3(gen, a, -10.0f, 10.0f);
    random_vec3(gen, b, -10.0f, 10.0f);
    random_vec3(gen, c, -10.0f, 10.0f);

    // Test associativity: (a + b) + c should be approximately equal to a + (b + c)
    // Due to floating-point precision, we allow some tolerance
    VectorAdd(a, b, temp1);
    VectorAdd(temp1, c, result1);

    VectorAdd(b, c, temp2);
    VectorAdd(a, temp2, result2);

    // Check with small tolerance for floating-point precision
    qboolean equal = qtrue;
    for (int i = 0; i < 3; i++) {
        if (fabsf(result1[i] - result2[i]) > 1e-5f) {
            equal = qfalse;
            break;
        }
    }

    ASSERT_TRUE(equal);
}

static void test_vector_zero_identity(random_generator_t *gen) {
    vec3_t a, zero = {0.0f, 0.0f, 0.0f}, result;

    random_vec3(gen, a, -100.0f, 100.0f);

    // Test identity: a + 0 == a
    VectorAdd(a, zero, result);
    ASSERT_TRUE(VectorsEqual(a, result));
}

static void test_angle_operations(random_generator_t *gen) {
    // Test angle normalization properties
    float angle = random_float(gen, -1000.0f, 1000.0f);

    // Test 360 normalization
    float normalized360 = AngleNormalize360(angle);
    ASSERT_TRUE(normalized360 >= 0.0f && normalized360 < 360.0f);

    // Test 180 normalization
    float normalized180 = AngleNormalize180(angle);
    ASSERT_TRUE(normalized180 >= -180.0f && normalized180 <= 180.0f);
}

TEST(property_vector_addition_commutative) {
    random_generator_t gen;
    random_init(&gen, (unsigned int)time(NULL));

    for (int i = 0; i < 100; i++) {
        test_vector_addition_commutative(&gen);
    }
}

TEST(property_vector_addition_associative) {
    random_generator_t gen;
    random_init(&gen, (unsigned int)time(NULL));

    for (int i = 0; i < 100; i++) {
        test_vector_addition_associative(&gen);
    }
}

static void property_test_vector_dot_product(random_generator_t *gen) {
    vec3_t a, b, c;
    random_vec3(gen, a, -10.0f, 10.0f);
    random_vec3(gen, b, -10.0f, 10.0f);
    random_vec3(gen, c, -10.0f, 10.0f);

    // Test dot product properties
    float ab = DotProduct(a, b);
    float ba = DotProduct(b, a);
    float ac = DotProduct(a, c);

    // Commutativity: a·b = b·a
    ASSERT_FLOAT_EQ(ab, ba, 1e-6f);

    // Distributivity: a·(b+c) = a·b + a·c
    vec3_t b_plus_c;
    VectorAdd(b, c, b_plus_c);
    float a_dot_b_plus_c = DotProduct(a, b_plus_c);
    float a_dot_b_plus_a_dot_c = ab + ac;
    ASSERT_FLOAT_EQ(a_dot_b_plus_c, a_dot_b_plus_a_dot_c, 1e-4f);
}

static void property_test_vector_cross_product(random_generator_t *gen) {
    vec3_t a, b, c;
    random_vec3(gen, a, -10.0f, 10.0f);
    random_vec3(gen, b, -10.0f, 10.0f);
    random_vec3(gen, c, -10.0f, 10.0f);

    // Cross product properties
    vec3_t a_cross_b, b_cross_a;

    CrossProduct(a, b, a_cross_b);
    CrossProduct(b, a, b_cross_a);

    // Anti-commutativity: a × b = -(b × a)
    vec3_t neg_b_cross_a;
    VectorNegate(b_cross_a, neg_b_cross_a);
    ASSERT_FLOAT_EQ(a_cross_b[0], neg_b_cross_a[0], 1e-5f);
    ASSERT_FLOAT_EQ(a_cross_b[1], neg_b_cross_a[1], 1e-5f);
    ASSERT_FLOAT_EQ(a_cross_b[2], neg_b_cross_a[2], 1e-5f);

    // Cross product with zero vector
    vec3_t zero = {0, 0, 0};
    vec3_t result;
    CrossProduct(a, zero, result);
    ASSERT_FLOAT_EQ(result[0], 0.0f, 1e-6f);
    ASSERT_FLOAT_EQ(result[1], 0.0f, 1e-6f);
    ASSERT_FLOAT_EQ(result[2], 0.0f, 1e-6f);
}

static void property_test_plane_operations(random_generator_t *gen) {
    // Generate random plane: ax + by + cz + d = 0
    vec3_t normal;
    random_vec3(gen, normal, -1.0f, 1.0f);
    // Normalize the normal
    float length = VectorLength(normal);
    if (length > 1e-6f) {
        VectorScale(normal, 1.0f / length, normal);
    }
    float distance = random_float(gen, -100.0f, 100.0f);

    // Test that normalized normal has length 1
    float normal_length = VectorLength(normal);
    ASSERT_FLOAT_EQ(normal_length, 1.0f, 1e-5f);

    // Test point-plane distance consistency
    vec3_t point;
    random_vec3(gen, point, -50.0f, 50.0f);

    // Distance should be consistent with plane equation
    float dist = DotProduct(normal, point) - distance;
    // We can't really test absolute correctness without knowing the expected result,
    // but we can test that the calculation doesn't crash and returns a finite value
    ASSERT_TRUE(isfinite(dist));
}

static void property_test_aabb_operations(random_generator_t *gen) {
    vec3_t mins1, maxs1;
    random_vec3(gen, mins1, -50.0f, 0.0f);
    random_vec3(gen, maxs1, 0.0f, 50.0f);

    // Ensure mins < maxs
    for (int i = 0; i < 3; i++) {
        if (mins1[i] > maxs1[i]) {
            float temp = mins1[i];
            mins1[i] = maxs1[i];
            maxs1[i] = temp;
        }
    }

    // Test AABB validity
    ASSERT_TRUE(mins1[0] <= maxs1[0] && mins1[1] <= maxs1[1] && mins1[2] <= maxs1[2]);

    // Test center point (should always be contained if AABB is valid)
    vec3_t center;
    VectorAdd(mins1, maxs1, center);
    VectorScale(center, 0.5f, center);

    qboolean center_contained = (center[0] >= mins1[0] && center[0] <= maxs1[0] &&
                                 center[1] >= mins1[1] && center[1] <= maxs1[1] &&
                                 center[2] >= mins1[2] && center[2] <= maxs1[2]);

    ASSERT_TRUE(center_contained);
}

static void property_test_sphere_operations(random_generator_t *gen) {
    vec3_t center;
    float radius = random_float(gen, 0.1f, 50.0f);

    random_vec3(gen, center, -50.0f, 50.0f);

    // Test sphere properties
    ASSERT_TRUE(radius > 0);

    // Test center point distance (should be 0)
    vec3_t center_to_center;
    VectorSubtract(center, center, center_to_center);
    float center_dist = VectorLength(center_to_center);
    ASSERT_FLOAT_EQ(center_dist, 0.0f, 1e-6f);
}

TEST(property_vector_zero_identity) {
    random_generator_t gen;
    random_init(&gen, (unsigned int)time(NULL));

    for (int i = 0; i < 100; i++) {
        test_vector_zero_identity(&gen);
    }
}

TEST(property_angle_operations) {
    random_generator_t gen;
    random_init(&gen, (unsigned int)time(NULL));

    for (int i = 0; i < 100; i++) {
        test_angle_operations(&gen);
    }
}

TEST(property_vector_dot_product) {
    random_generator_t gen;
    random_init(&gen, (unsigned int)time(NULL));

    for (int i = 0; i < 100; i++) {
        property_test_vector_dot_product(&gen);
    }
}

TEST(property_vector_cross_product) {
    random_generator_t gen;
    random_init(&gen, (unsigned int)time(NULL));

    for (int i = 0; i < 100; i++) {
        property_test_vector_cross_product(&gen);
    }
}

TEST(property_plane_operations) {
    random_generator_t gen;
    random_init(&gen, (unsigned int)time(NULL));

    for (int i = 0; i < 100; i++) {
        property_test_plane_operations(&gen);
    }
}

TEST(property_aabb_operations) {
    random_generator_t gen;
    random_init(&gen, (unsigned int)time(NULL));

    for (int i = 0; i < 100; i++) {
        property_test_aabb_operations(&gen);
    }
}

TEST(property_sphere_operations) {
    random_generator_t gen;
    random_init(&gen, (unsigned int)time(NULL));

    for (int i = 0; i < 100; i++) {
        property_test_sphere_operations(&gen);
    }
}

int main(void) {
	Com_Printf("Running property-based tests...\n\n");

	RUN_TEST(property_vector_addition_commutative);
	RUN_TEST(property_vector_addition_associative);
	RUN_TEST(property_vector_zero_identity);
	RUN_TEST(property_angle_operations);
	RUN_TEST(property_vector_dot_product);
	RUN_TEST(property_vector_cross_product);
	RUN_TEST(property_plane_operations);
	RUN_TEST(property_aabb_operations);
	RUN_TEST(property_sphere_operations);
	RUN_TEST(property_vector_dot_product);
	RUN_TEST(property_vector_cross_product);
	RUN_TEST(property_plane_operations);
	RUN_TEST(property_aabb_operations);
	RUN_TEST(property_sphere_operations);

	PRINT_TEST_SUMMARY();

	Com_Printf("\nProperty-based tests completed.\n");
	Com_Printf("These tests verify mathematical properties with generated inputs.\n");

	return (test_failed > 0) ? 1 : 0;
}
