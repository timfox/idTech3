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

void Com_DPrintf(const char *fmt, ...) {
	(void)fmt; // Debug printf - do nothing in tests
}

fileHandle_t FS_FOpenFileRead(const char *filename, fileHandle_t *file, qboolean uniqueFILE) {
	(void)filename;
	(void)file;
	(void)uniqueFILE;
	return 0; // Not implemented for tests
}

int FS_Read(void *buffer, int len, fileHandle_t f) {
	(void)buffer;
	(void)len;
	(void)f;
	return 0; // Not implemented for tests
}

void FS_FCloseFile(fileHandle_t f) {
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

    if (!VectorsEqual(result1, result2)) {
        Com_Printf("Vector addition commutativity failed\n");
        test_failed++;
    }
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

    if (!equal) {
        Com_Printf("Vector addition associativity failed (within tolerance)\n");
        test_failed++;
    }
}

static void test_vector_zero_identity(random_generator_t *gen) {
    vec3_t a, zero = {0.0f, 0.0f, 0.0f}, result;

    random_vec3(gen, a, -100.0f, 100.0f);

    // Test identity: a + 0 == a
    VectorAdd(a, zero, result);
    if (!VectorsEqual(a, result)) {
        Com_Printf("Vector zero identity failed\n");
        test_failed++;
    }
}

static void test_angle_operations(random_generator_t *gen) {
    // Test angle normalization properties
    float angle = random_float(gen, -1000.0f, 1000.0f);

    // Test 360 normalization
    float normalized360 = AngleNormalize360(angle);
    if (normalized360 < 0.0f || normalized360 >= 360.0f) {
        Com_Printf("Angle 360 normalization failed\n");
        test_failed++;
    }

    // Test 180 normalization
    float normalized180 = AngleNormalize180(angle);
    if (normalized180 < -180.0f || normalized180 > 180.0f) {
        Com_Printf("Angle 180 normalization failed\n");
        test_failed++;
    }
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

int main(void) {
	Com_Printf("Running property-based tests...\n\n");

	RUN_TEST(property_vector_addition_commutative);
	RUN_TEST(property_vector_addition_associative);
	RUN_TEST(property_vector_zero_identity);
	RUN_TEST(property_angle_operations);

	PRINT_TEST_SUMMARY();

	Com_Printf("\nProperty-based tests completed.\n");
	Com_Printf("These tests verify mathematical properties with generated inputs.\n");

	return (test_failed > 0) ? 1 : 0;
}
