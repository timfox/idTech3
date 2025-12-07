/*
===========================================================================
Basic math utilities tests for q_math.c
===========================================================================
*/

#include "test_framework.h"
#include "../src/qcommon/q_shared.h"
#include <math.h>

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

int main(void) {
	Com_Printf("Running qmath tests...\n\n");

	RUN_TEST(vectornormalize2_normalizes_and_returns_length);
	RUN_TEST(vectornormalize2_zero_vector);
	RUN_TEST(vector_operations_dot_and_ma);
	RUN_TEST(bounds_intersect_helpers);

	PRINT_TEST_SUMMARY();
	return (test_failed > 0) ? 1 : 0;
}


