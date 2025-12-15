/*
===========================================================================
Memory Management Tests
===========================================================================
*/

#include <string.h>
#include <stdlib.h>
#include <math.h>

// Ensure sqrtf is available
#ifndef sqrtf
#define sqrtf sqrt
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

// Forward declarations and type definitions for testing
typedef enum {
	ERR_FATAL,
	ERR_DROP,
	ERR_SERVERDISCONNECT,
	ERR_DISCONNECT,
	ERR_NEED_CD
} errorParm_t;

typedef int memtag_t;
#define TAG_GENERAL 0

// Minimal test framework for standalone testing
static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define ASSERT_EQ(a, b) \
	do { \
		test_count++; \
		if ((a) != (b)) { \
			printf("FAIL: %s:%d: Expected %d, got %d\n", \
				__func__, __LINE__, (int)(b), (int)(a)); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define ASSERT_NE(a, b) \
	do { \
		test_count++; \
		if ((a) == (b)) { \
			printf("FAIL: %s:%d: Expected not equal, got %p\n", \
				__func__, __LINE__, (void *)(a)); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define ASSERT_STR_EQ(a, b) \
	do { \
		test_count++; \
		if (strcmp((a), (b)) != 0) { \
			printf("FAIL: %s:%d: Expected \"%s\", got \"%s\"\n", \
				__func__, __LINE__, (b), (a)); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define ASSERT_FLOAT_EQ(a, b, tolerance) \
	do { \
		test_count++; \
		if (fabsf((a) - (b)) > (tolerance)) { \
			printf("FAIL: %s:%d: Expected %f (±%f), got %f\n", \
				__func__, __LINE__, (b), (tolerance), (a)); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define ASSERT_NOT_NULL(ptr) \
	do { \
		test_count++; \
		if ((ptr) == NULL) { \
			printf("FAIL: %s:%d: Expected non-NULL pointer\n", \
				__func__, __LINE__); \
			test_failed++; \
			return; \
		} \
		test_passed++; \
	} while(0)

#define TEST(name) static void test_##name(void)

#define RUN_TEST(name) \
	do { \
		printf("Running test: %s\n", #name); \
		test_##name(); \
	} while(0)

#define PRINT_TEST_SUMMARY() \
	do { \
		printf("\n=== Test Summary ===\n"); \
		printf("Total: %d\n", test_count); \
		printf("Passed: %d\n", test_passed); \
		printf("Failed: %d\n", test_failed); \
		if (test_failed == 0) { \
			printf("All tests passed!\n"); \
		} \
	} while(0)

// Minimal stubs for test framework
// Forward declarations
void Com_Printf(const char *fmt, ...);
void Com_Error(errorParm_t level, const char *error, ...);
float Q_atof(const char *str);
void *Z_TagMalloc(int size, memtag_t tag);
void Z_Free(void *ptr);

void Com_Printf(const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

// Stub for Com_Error
void Com_Error(errorParm_t level, const char *error, ...) {
	(void)level;  // Unused parameter
	va_list argptr;
	va_start(argptr, error);
	vfprintf(stderr, error, argptr);
	va_end(argptr);
	exit(1);
}

// Stub for Q_atof
float Q_atof(const char *str) {
	return atof(str);
}

// Minimal vector and string functions for testing
typedef float vec3_t[3];
#define VectorNormalize2(v, out) VectorNormalize2_impl(v, out)
static float VectorNormalize2_impl(const vec3_t v, vec3_t out) {
	float length = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
	if (length > 0.0f) {
		float invLength = 1.0f / length;
		out[0] = v[0] * invLength;
		out[1] = v[1] * invLength;
		out[2] = v[2] * invLength;
	} else {
		out[0] = out[1] = out[2] = 0.0f;
	}
	return length;
}

#define DotProduct(a, b) ((a)[0]*(b)[0] + (a)[1]*(b)[1] + (a)[2]*(b)[2])

#define _VectorMA(a, scale, b, out) \
	(out)[0] = (a)[0] + (scale)*(b)[0]; \
	(out)[1] = (a)[1] + (scale)*(b)[1]; \
	(out)[2] = (a)[2] + (scale)*(b)[2]

#define BoundsIntersectPoint(mins, maxs, point) \
	((point)[0] >= (mins)[0] && (point)[0] <= (maxs)[0] && \
	 (point)[1] >= (mins)[1] && (point)[1] <= (maxs)[1] && \
	 (point)[2] >= (mins)[2] && (point)[2] <= (maxs)[2])

#define BoundsIntersectSphere(mins, maxs, origin, radius) \
	((origin)[0] - (radius) >= (mins)[0] && (origin)[0] + (radius) <= (maxs)[0] && \
	 (origin)[1] - (radius) >= (mins)[1] && (origin)[1] + (radius) <= (maxs)[1] && \
	 (origin)[2] - (radius) >= (mins)[2] && (origin)[2] + (radius) <= (maxs)[2])

// Test stub for memory tag allocation (simplified)
void *Z_TagMalloc(int size, memtag_t tag) {
	(void)tag;  // Unused parameter
	return malloc(size);
}

void Z_Free(void *ptr) {
	free(ptr);
}

TEST(memory_basic_allocation) {
	void *ptr1 = Z_TagMalloc(100, TAG_GENERAL);
	void *ptr2 = Z_TagMalloc(200, TAG_GENERAL);

	ASSERT_NOT_NULL(ptr1);
	ASSERT_NOT_NULL(ptr2);
	ASSERT_NE(ptr1, ptr2);

	// Fill with test data
	memset(ptr1, 0xAA, 100);
	memset(ptr2, 0xBB, 200);

	// Verify data integrity
	for (int i = 0; i < 100; i++) {
		ASSERT_EQ(((unsigned char *)ptr1)[i], 0xAA);
	}
	for (int i = 0; i < 200; i++) {
		ASSERT_EQ(((unsigned char *)ptr2)[i], 0xBB);
	}

	Z_Free(ptr1);
	Z_Free(ptr2);
}

TEST(memory_string_operations) {
	// Test string allocation and copying
	char *testString = "Hello, World!";
	size_t len = strlen(testString) + 1;

	char *copied = Z_TagMalloc(len, TAG_GENERAL);
	ASSERT_NOT_NULL(copied);

	strcpy(copied, testString);
	ASSERT_STR_EQ(copied, testString);

	Z_Free(copied);
}

TEST(memory_zero_allocation) {
	// Test zero-sized allocation (should work or return NULL)
	void *ptr = Z_TagMalloc(0, TAG_GENERAL);
	// Zero-sized allocations may return NULL or a valid pointer
	// Either is acceptable behavior
	if (ptr != NULL) {
		Z_Free(ptr);
	}
}

TEST(memory_large_allocation) {
	// Test larger allocation
	const size_t largeSize = 1024 * 1024; // 1MB
	void *ptr = Z_TagMalloc(largeSize, TAG_GENERAL);

	if (ptr != NULL) {
		// Fill with pattern
		memset(ptr, 0xCC, largeSize);

		// Verify pattern
		for (size_t i = 0; i < largeSize; i += 1024) { // Check every 1KB
			ASSERT_EQ(((unsigned char *)ptr)[i], 0xCC);
		}

		Z_Free(ptr);
	} else {
		// Large allocation failed - this might be expected in constrained environments
		Com_Printf("Large allocation test skipped (allocation failed)\n");
	}
}

TEST(memory_vector_operations) {
	// Test vector normalization function
	vec3_t input = {3.0f, 4.0f, 0.0f};
	vec3_t output;

	float originalLength = VectorNormalize2(input, output);

	// Should return original length (5.0)
	ASSERT_FLOAT_EQ(originalLength, 5.0f, 0.001f);

	// Output should be normalized (length = 1.0)
	float normalizedLength = sqrtf(output[0]*output[0] + output[1]*output[1] + output[2]*output[2]);
	ASSERT_FLOAT_EQ(normalizedLength, 1.0f, 0.001f);

	// Check normalized components
	ASSERT_FLOAT_EQ(output[0], 0.6f, 0.001f); // 3/5
	ASSERT_FLOAT_EQ(output[1], 0.8f, 0.001f); // 4/5
	ASSERT_FLOAT_EQ(output[2], 0.0f, 0.001f);
}

TEST(memory_alignment) {
	// Test that allocations are reasonably aligned
	void *ptr = Z_TagMalloc(100, TAG_GENERAL);
	ASSERT_NOT_NULL(ptr);

	// Check basic alignment (should be at least 4-byte aligned)
	uintptr_t addr = (uintptr_t)ptr;
	ASSERT_EQ(addr % 4, 0);

	Z_Free(ptr);
}

int main(void) {
	Com_Printf("Running memory management tests...\n\n");

	RUN_TEST(memory_basic_allocation);
	RUN_TEST(memory_string_operations);
	RUN_TEST(memory_zero_allocation);
	RUN_TEST(memory_large_allocation);
	RUN_TEST(memory_alignment);

	PRINT_TEST_SUMMARY();
	return (test_failed > 0) ? 1 : 0;
}