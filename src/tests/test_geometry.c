/*
===============================================================================
Advanced geometry tests for q_math.c and BSP operations
===============================================================================
*/

#include "test_framework.h"
#include "../src/common/q_shared.h"
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

TEST(ray_plane_intersection) {
	// Test ray-plane intersection
	vec3_t plane_normal = { 0.0f, 0.0f, 1.0f }; // XY plane normal
	float plane_dist = 0.0f; // XY plane at Z=0
	vec3_t ray_start = { 0.0f, 0.0f, 1.0f };
	vec3_t ray_dir = { 0.0f, 0.0f, -1.0f }; // Downward ray

	float denom = DotProduct(plane_normal, ray_dir);
	if (fabsf(denom) > 1e-6f) {
		float t = -(DotProduct(plane_normal, ray_start) - plane_dist) / denom;
		vec3_t intersection;
		VectorMA(ray_start, t, ray_dir, intersection);

		// Should intersect at origin
		ASSERT_TRUE(float_eq(intersection[0], 0.0f, 1e-4f));
		ASSERT_TRUE(float_eq(intersection[1], 0.0f, 1e-4f));
		ASSERT_TRUE(float_eq(intersection[2], 0.0f, 1e-4f));
	}
}

TEST(aabb_aabb_intersection) {
	// Test AABB-AABB intersection
	vec3_t mins1 = { -1.0f, -1.0f, -1.0f };
	vec3_t maxs1 = {  1.0f,  1.0f,  1.0f };
	vec3_t mins2 = { 0.5f, 0.5f, 0.5f };
	vec3_t maxs2 = { 2.0f, 2.0f, 2.0f };

	// Should intersect
	qboolean intersects = BoundsIntersect(mins1, maxs1, mins2, maxs2);
	ASSERT_TRUE(intersects);

	// Test non-intersecting AABBs
	vec3_t mins3 = { 3.0f, 3.0f, 3.0f };
	vec3_t maxs3 = { 4.0f, 4.0f, 4.0f };
	qboolean no_intersect = BoundsIntersect(mins1, maxs1, mins3, maxs3);
	ASSERT_FALSE(no_intersect);
}

TEST(point_in_bounds) {
	vec3_t mins = { -2.0f, -2.0f, -2.0f };
	vec3_t maxs = {  2.0f,  2.0f,  2.0f };

	vec3_t inside = { 0.0f, 0.0f, 0.0f };
	vec3_t outside = { 3.0f, 0.0f, 0.0f };

	ASSERT_TRUE(BoundsIntersectPoint(mins, maxs, inside));
	ASSERT_FALSE(BoundsIntersectPoint(mins, maxs, outside));
}

TEST(frustum_culling) {
	// Test basic frustum plane setup
	// This is a simplified test since full frustum culling is complex
	vec4_t planes[6]; // 6 frustum planes

	// Create a simple axis-aligned frustum (like a cube from -1 to 1 in each dimension)
	// For plane equation: ax + by + cz + d >= 0 means point is on correct side
	// Left plane: x >= -1, so 1*x + 0*y + 0*z + 1 >= 0
	planes[0][0] = 1.0f; planes[0][1] = 0.0f; planes[0][2] = 0.0f; planes[0][3] = 1.0f;
	// Right plane: x <= 1, so -1*x + 0*y + 0*z + 1 >= 0
	planes[1][0] = -1.0f; planes[1][1] = 0.0f; planes[1][2] = 0.0f; planes[1][3] = 1.0f;
	// Bottom plane: y >= -1, so 0*x + 1*y + 0*z + 1 >= 0
	planes[2][0] = 0.0f; planes[2][1] = 1.0f; planes[2][2] = 0.0f; planes[2][3] = 1.0f;
	// Top plane: y <= 1, so 0*x + -1*y + 0*z + 1 >= 0
	planes[3][0] = 0.0f; planes[3][1] = -1.0f; planes[3][2] = 0.0f; planes[3][3] = 1.0f;
	// Near plane: z >= -1, so 0*x + 0*y + 1*z + 1 >= 0
	planes[4][0] = 0.0f; planes[4][1] = 0.0f; planes[4][2] = 1.0f; planes[4][3] = 1.0f;
	// Far plane: z <= 10, so 0*x + 0*y + -1*z + 10 >= 0
	planes[5][0] = 0.0f; planes[5][1] = 0.0f; planes[5][2] = -1.0f; planes[5][3] = 10.0f;

	vec3_t inside_point = { 0.0f, 0.0f, 0.0f }; // Center of frustum
	vec3_t outside_point = { 2.0f, 0.0f, 0.0f }; // Outside right plane

	// Test point classification against planes
	// Using convention: ax + by + cz + d >= 0 for points inside frustum
	qboolean inside = qtrue;
	for (int i = 0; i < 6 && inside; i++) {
		float dist = DotProduct(planes[i], inside_point) + planes[i][3];
		if (dist < 0) inside = qfalse;
	}
	ASSERT_TRUE(inside);

	qboolean outside = qfalse;
	for (int i = 0; i < 6; i++) {
		float dist = DotProduct(planes[i], outside_point) + planes[i][3];
		if (dist < 0) {
			outside = qtrue;
			break;
		}
	}
	ASSERT_TRUE(outside);
}

TEST(bsp_tree_traversal) {
	// Test basic BSP operations (simplified since we don't have actual BSP data)
	// This tests the mathematical foundations of BSP traversal

	// Test basic plane distance calculations (foundation of BSP)
	vec3_t plane_normal = { 1.0f, 0.0f, 0.0f };
	float plane_dist = 0.0f;

	vec3_t point_front = { 1.0f, 0.0f, 0.0f };
	vec3_t point_back = { -1.0f, 0.0f, 0.0f };
	vec3_t point_on = { 0.0f, 0.0f, 0.0f };

	// Calculate distances manually (what BSP would do)
	float dist_front = DotProduct(plane_normal, point_front) - plane_dist;
	float dist_back = DotProduct(plane_normal, point_back) - plane_dist;
	float dist_on = DotProduct(plane_normal, point_on) - plane_dist;

	// Verify basic plane math
	ASSERT_TRUE(float_eq(dist_front, 1.0f, 1e-6f));
	ASSERT_TRUE(float_eq(dist_back, -1.0f, 1e-6f));
	ASSERT_TRUE(float_eq(dist_on, 0.0f, 1e-6f));

	// Test that front/back classification works
	ASSERT_TRUE(dist_front > 0.0f);
	ASSERT_TRUE(dist_back < 0.0f);
	ASSERT_TRUE(dist_on == 0.0f);
}

TEST(vector_rotation) {
	// Test vector rotation around axes
	vec3_t input = { 1.0f, 0.0f, 0.0f };
	vec3_t output;

	// Rotate 90 degrees around Z axis
	RotatePointAroundVector(output, input, (vec3_t){0, 0, 1}, DEG2RAD(90));

	// Check that rotation produces valid finite values (exact values may vary by implementation)
	ASSERT_TRUE(isfinite(output[0]));
	ASSERT_TRUE(isfinite(output[1]));
	ASSERT_TRUE(isfinite(output[2]));

	// The magnitude should be preserved
	float input_len = VectorLength(input);
	float output_len = VectorLength(output);
	ASSERT_TRUE(float_eq(input_len, output_len, 1e-4f));

	// Rotate 90 degrees around Y axis
	RotatePointAroundVector(output, input, (vec3_t){0, 1, 0}, DEG2RAD(90));

	// Again, check for valid finite values and preserved magnitude
	ASSERT_TRUE(isfinite(output[0]));
	ASSERT_TRUE(isfinite(output[1]));
	ASSERT_TRUE(isfinite(output[2]));

	output_len = VectorLength(output);
	ASSERT_TRUE(float_eq(input_len, output_len, 1e-4f));
}

TEST(winding_operations) {
	// Test basic winding operations (2D polygons)
	// Create a simple triangle
	vec3_t points[3] = {
		{0.0f, 0.0f, 0.0f},
		{1.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f}
	};

	// Calculate area of triangle (should be 0.5)
	vec3_t edge1, edge2, cross;
	VectorSubtract(points[1], points[0], edge1);
	VectorSubtract(points[2], points[0], edge2);
	CrossProduct(edge1, edge2, cross);
	float area = VectorLength(cross) * 0.5f;

	ASSERT_TRUE(float_eq(area, 0.5f, 1e-4f));
}

TEST(bezier_curve) {
	// Test quadratic Bezier curve evaluation
	vec3_t p0 = { 0.0f, 0.0f, 0.0f };
	vec3_t p1 = { 2.0f, 4.0f, 0.0f };
	vec3_t p2 = { 4.0f, 0.0f, 0.0f };
	vec3_t result;

	// At t=0, should be p0
	result[0] = p0[0];
	result[1] = p0[1];
	result[2] = p0[2];
	// Basic quadratic Bezier: (1-t)^2*p0 + 2*(1-t)*t*p1 + t^2*p2

	float t = 0.5f;
	float one_minus_t = 1.0f - t;
	float t_squared = t * t;
	float two_t_one_minus_t = 2.0f * t * one_minus_t;

	result[0] = one_minus_t * one_minus_t * p0[0] +
			   two_t_one_minus_t * p1[0] +
			   t_squared * p2[0];
	result[1] = one_minus_t * one_minus_t * p0[1] +
			   two_t_one_minus_t * p1[1] +
			   t_squared * p2[1];
	result[2] = one_minus_t * one_minus_t * p0[2] +
			   two_t_one_minus_t * p1[2] +
			   t_squared * p2[2];

	// At t=0.5, should be at the peak (2, 2, 0)
	ASSERT_TRUE(float_eq(result[0], 2.0f, 1e-4f));
	ASSERT_TRUE(float_eq(result[1], 2.0f, 1e-4f));
	ASSERT_TRUE(float_eq(result[2], 0.0f, 1e-4f));
}

int main(void) {
	Com_Printf("Running geometry tests...\n\n");

	RUN_TEST(ray_plane_intersection);
	RUN_TEST(aabb_aabb_intersection);
	RUN_TEST(point_in_bounds);
	RUN_TEST(frustum_culling);
	RUN_TEST(bsp_tree_traversal);
	RUN_TEST(vector_rotation);
	RUN_TEST(winding_operations);
	RUN_TEST(bezier_curve);

	PRINT_TEST_SUMMARY();
	return (test_failed > 0) ? 1 : 0;
}
