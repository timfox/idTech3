/*
===========================================================================
Basic math utilities tests for q_math.c
===========================================================================
*/

#include "test_framework.h"
#include "../src/qcommon/q_shared.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

// Mock Com_Error for testing
void Com_Error(errorParm_t level, const char *error, ...) {
	(void)level;  // Unused parameter
	va_list argptr;
	va_start(argptr, error);
	vfprintf(stderr, error, argptr);
	va_end(argptr);
	fprintf(stderr, "\n");
	exit(1);
}

// Minimal Com_Printf stub for the test framework
void Com_Printf(const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

static qboolean float_eq(float a, float b, float eps) {
	return fabsf(a - b) <= eps;
}

TEST(vectornormalize2_normalizes_and_returns_length) {
	vec3_t in = { 3.0f, 4.0f, 0.0f };
	vec3_t out;
	float len = VectorNormalize2(in, out);

	ASSERT_TRUE(float_eq(len, 5.0f, 1e-4f));
	ASSERT_TRUE(float_eq(out[0], 0.6f, 1e-4f));
	ASSERT_TRUE(float_eq(out[1], 0.8f, 1e-4f));
	ASSERT_TRUE(float_eq(out[2], 0.0f, 1e-4f));
}

TEST(vectornormalize2_zero_vector) {
	vec3_t in = { 0.0f, 0.0f, 0.0f };
	vec3_t out = { 1.0f, 2.0f, 3.0f };
	float len = VectorNormalize2(in, out);

	ASSERT_TRUE(float_eq(len, 0.0f, 1e-6f));
	ASSERT_TRUE(float_eq(out[0], 0.0f, 1e-6f));
	ASSERT_TRUE(float_eq(out[1], 0.0f, 1e-6f));
	ASSERT_TRUE(float_eq(out[2], 0.0f, 1e-6f));
}

TEST(vector_operations_dot_and_ma) {
	vec3_t a = { 1.0f, 2.0f, 3.0f };
	vec3_t b = { -4.0f, 5.0f, -6.0f };

	float dot = DotProduct(a, b);
	ASSERT_TRUE(float_eq(dot, -12.0f, 1e-4f)); // 1*-4 + 2*5 + 3*-6

	vec3_t out;
	_VectorMA(a, 2.0f, b, out); // out = a + 2*b
	ASSERT_TRUE(float_eq(out[0], -7.0f, 1e-4f));
	ASSERT_TRUE(float_eq(out[1], 12.0f, 1e-4f));
	ASSERT_TRUE(float_eq(out[2], -9.0f, 1e-4f));
}

TEST(bounds_intersect_helpers) {
	vec3_t mins = { -1.0f, -1.0f, -1.0f };
	vec3_t maxs = {  1.0f,  1.0f,  1.0f };

	vec3_t point_inside = { 0.5f, 0.0f, -0.25f };
	vec3_t point_outside = { 2.0f, 0.0f, 0.0f };
	ASSERT_TRUE(BoundsIntersectPoint(mins, maxs, point_inside));
	ASSERT_FALSE(BoundsIntersectPoint(mins, maxs, point_outside));

	vec3_t origin_inside = { 0.0f, 0.0f, 0.0f };
	vec3_t origin_touching = { 1.5f, 0.0f, 0.0f };
	ASSERT_TRUE(BoundsIntersectSphere(mins, maxs, origin_inside, 0.5f));
	ASSERT_TRUE(BoundsIntersectSphere(mins, maxs, origin_touching, 0.5f)); // tangent is allowed
	ASSERT_FALSE(BoundsIntersectSphere(mins, maxs, origin_touching, 0.1f));
}

TEST(vector_cross_product) {
	vec3_t a = { 1.0f, 0.0f, 0.0f };
	vec3_t b = { 0.0f, 1.0f, 0.0f };
	vec3_t cross;
	CrossProduct(a, b, cross);
	
	ASSERT_TRUE(float_eq(cross[0], 0.0f, 1e-4f));
	ASSERT_TRUE(float_eq(cross[1], 0.0f, 1e-4f));
	ASSERT_TRUE(float_eq(cross[2], 1.0f, 1e-4f)); // Right-hand rule: x × y = z
	
	// Test anti-commutativity: a × b = -(b × a)
	vec3_t cross_reverse;
	CrossProduct(b, a, cross_reverse);
	ASSERT_TRUE(float_eq(cross[0], -cross_reverse[0], 1e-4f));
	ASSERT_TRUE(float_eq(cross[1], -cross_reverse[1], 1e-4f));
	ASSERT_TRUE(float_eq(cross[2], -cross_reverse[2], 1e-4f));
}

TEST(vector_length_and_distance) {
	vec3_t v = { 3.0f, 4.0f, 0.0f };
	float len = VectorLength(v);
	ASSERT_TRUE(float_eq(len, 5.0f, 1e-4f));
	
	float len_sq = VectorLengthSquared(v);
	ASSERT_TRUE(float_eq(len_sq, 25.0f, 1e-4f));
	
	vec3_t p1 = { 0.0f, 0.0f, 0.0f };
	vec3_t p2 = { 3.0f, 4.0f, 0.0f };
	float dist = Distance(p1, p2);
	ASSERT_TRUE(float_eq(dist, 5.0f, 1e-4f));
}

TEST(vector_add_subtract_scale) {
	vec3_t a = { 1.0f, 2.0f, 3.0f };
	vec3_t b = { 4.0f, 5.0f, 6.0f };
	vec3_t result;
	
	VectorAdd(a, b, result);
	ASSERT_TRUE(float_eq(result[0], 5.0f, 1e-4f));
	ASSERT_TRUE(float_eq(result[1], 7.0f, 1e-4f));
	ASSERT_TRUE(float_eq(result[2], 9.0f, 1e-4f));
	
	VectorSubtract(b, a, result);
	ASSERT_TRUE(float_eq(result[0], 3.0f, 1e-4f));
	ASSERT_TRUE(float_eq(result[1], 3.0f, 1e-4f));
	ASSERT_TRUE(float_eq(result[2], 3.0f, 1e-4f));
	
	VectorScale(a, 2.0f, result);
	ASSERT_TRUE(float_eq(result[0], 2.0f, 1e-4f));
	ASSERT_TRUE(float_eq(result[1], 4.0f, 1e-4f));
	ASSERT_TRUE(float_eq(result[2], 6.0f, 1e-4f));
}

TEST(vector_copy_and_set) {
	vec3_t src = { 1.0f, 2.0f, 3.0f };
	vec3_t dst = { 0.0f, 0.0f, 0.0f };
	
	VectorCopy(src, dst);
	ASSERT_TRUE(float_eq(dst[0], 1.0f, 1e-4f));
	ASSERT_TRUE(float_eq(dst[1], 2.0f, 1e-4f));
	ASSERT_TRUE(float_eq(dst[2], 3.0f, 1e-4f));
	
	VectorClear(dst);
	ASSERT_TRUE(float_eq(dst[0], 0.0f, 1e-4f));
	ASSERT_TRUE(float_eq(dst[1], 0.0f, 1e-4f));
	ASSERT_TRUE(float_eq(dst[2], 0.0f, 1e-4f));
	
	VectorSet(dst, 5.0f, 6.0f, 7.0f);
	ASSERT_TRUE(float_eq(dst[0], 5.0f, 1e-4f));
	ASSERT_TRUE(float_eq(dst[1], 6.0f, 1e-4f));
	ASSERT_TRUE(float_eq(dst[2], 7.0f, 1e-4f));
}

TEST(angle_normalization) {
	// Test angle normalization to 0-360 range
	float angle1 = AngleNormalize360(370.0f);
	// AngleNormalize360 uses a fixed-point wrap; allow small quantization error.
	ASSERT_TRUE(float_eq(angle1, 10.0f, 0.01f));
	
	float angle2 = AngleNormalize360(-10.0f);
	ASSERT_TRUE(float_eq(angle2, 350.0f, 0.01f));
	
	// Test angle normalization to -180 to 180 range
	float angle3 = AngleNormalize180(190.0f);
	ASSERT_TRUE(float_eq(angle3, -170.0f, 0.01f));
	
	float angle4 = AngleNormalize180(-190.0f);
	ASSERT_TRUE(float_eq(angle4, 170.0f, 0.01f));
}

TEST(angle_delta) {
	// Test angle difference calculation
	float delta1 = AngleDelta(10.0f, 20.0f);
	// AngleDelta returns AngleNormalize180(angle1 - angle2)
	ASSERT_TRUE(float_eq(delta1, -10.0f, 0.01f));
	
	float delta2 = AngleDelta(350.0f, 10.0f);
	// 350 - 10 = 340, normalized to -20
	ASSERT_TRUE(float_eq(delta2, -20.0f, 0.01f));
}

TEST(bounds_operations) {
	vec3_t mins, maxs;
	
	ClearBounds(mins, maxs);
	// After ClearBounds, mins should be very large positive, maxs very large negative
	ASSERT_TRUE(mins[0] > 1000.0f);
	ASSERT_TRUE(maxs[0] < -1000.0f);
	
	vec3_t point1 = { 1.0f, 2.0f, 3.0f };
	vec3_t point2 = { -1.0f, -2.0f, -3.0f };
	vec3_t point3 = { 5.0f, 0.0f, 0.0f };
	
	ClearBounds(mins, maxs);
	AddPointToBounds(point1, mins, maxs);
	AddPointToBounds(point2, mins, maxs);
	AddPointToBounds(point3, mins, maxs);
	
	ASSERT_TRUE(float_eq(mins[0], -1.0f, 1e-4f));
	ASSERT_TRUE(float_eq(mins[1], -2.0f, 1e-4f));
	ASSERT_TRUE(float_eq(mins[2], -3.0f, 1e-4f));
	ASSERT_TRUE(float_eq(maxs[0], 5.0f, 1e-4f));
	ASSERT_TRUE(float_eq(maxs[1], 2.0f, 1e-4f));
	ASSERT_TRUE(float_eq(maxs[2], 3.0f, 1e-4f));
}

int main(void) {
	Com_Printf("Running qmath tests...\n\n");

	RUN_TEST(vectornormalize2_normalizes_and_returns_length);
	RUN_TEST(vectornormalize2_zero_vector);
	RUN_TEST(vector_operations_dot_and_ma);
	RUN_TEST(bounds_intersect_helpers);
	RUN_TEST(vector_cross_product);
	RUN_TEST(vector_length_and_distance);
	RUN_TEST(vector_add_subtract_scale);
	RUN_TEST(vector_copy_and_set);
	RUN_TEST(angle_normalization);
	RUN_TEST(angle_delta);
	RUN_TEST(bounds_operations);

	PRINT_TEST_SUMMARY();
	return (test_failed > 0) ? 1 : 0;
}


