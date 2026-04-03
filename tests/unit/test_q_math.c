/*
 * Unit tests: q_math.c (stateless math helpers)
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/q_shared.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_NEAR(a, b, eps, msg) do { \
	if (fabsf((a) - (b)) > (eps)) { \
		fprintf(stderr, "FAIL: %s (got %f expected ~%f)\n", msg, (double)(a), (double)(b)); \
		return 1; \
	} \
} while (0)

static int test_clamp(void)
{
	ASSERT(ClampChar(0) == 0, "ClampChar(0)");
	ASSERT(ClampChar(127) == 127, "ClampChar(127)");
	ASSERT(ClampChar(-128) == -128, "ClampChar(-128)");
	ASSERT(ClampChar(200) == 127, "ClampChar overflow");
	ASSERT(ClampChar(-200) == -128, "ClampChar underflow");

	ASSERT(ClampCharMove(0) == 0, "ClampCharMove(0)");
	ASSERT(ClampCharMove(-127) == -127, "ClampCharMove(-127)");
	ASSERT(ClampCharMove(-128) == -127, "ClampCharMove(-128) clamps to -127");

	ASSERT(ClampShort(0) == 0, "ClampShort(0)");
	ASSERT(ClampShort(0x7fff) == 0x7fff, "ClampShort max");
	ASSERT(ClampShort(-32768) == -32768, "ClampShort min");
	return 0;
}

static int test_q_rand(void)
{
	int s = 1;
	int a = Q_rand(&s);
	int b = Q_rand(&s);
	ASSERT(a != b || a == 69070, "Q_rand advances"); /* 69069*1+1 */
	s = 1;
	(void)Q_rand(&s);
	(void)Q_rand(&s);
	float r = Q_random(&s);
	ASSERT(r >= 0.0f && r < 1.0f, "Q_random range");
	return 0;
}

static int test_color_bytes(void)
{
	unsigned u3 = ColorBytes3(1.0f, 0.5f, 0.0f);
	byte *p = (byte *)&u3;
	ASSERT(p[0] == 255 && p[1] == 127 && p[2] == 0, "ColorBytes3 channels");

	unsigned u4 = ColorBytes4(0.0f, 1.0f, 0.0f, 0.25f);
	p = (byte *)&u4;
	ASSERT(p[1] == 255 && p[3] == 63, "ColorBytes4 green/alpha"); /* 0.25*255 */
	return 0;
}

static int test_normalize_color(void)
{
	vec3_t in = { 2.0f, 1.0f, 0.5f };
	vec3_t out;
	float m = NormalizeColor(in, out);
	ASSERT_NEAR(m, 2.0f, 0.001f, "NormalizeColor max");
	ASSERT_NEAR(out[0], 1.0f, 0.001f, "NormalizeColor r");
	ASSERT_NEAR(out[1], 0.5f, 0.001f, "NormalizeColor g");
	ASSERT_NEAR(out[2], 0.25f, 0.001f, "NormalizeColor b");

	vec3_t z = { 0, 0, 0 };
	VectorClear(out);
	m = NormalizeColor(z, out);
	ASSERT(m == 0.0f, "NormalizeColor zero max");
	ASSERT(out[0] == 0 && out[1] == 0 && out[2] == 0, "NormalizeColor zero out");
	return 0;
}

static int test_plane_from_points(void)
{
	vec3_t a = { 0, 0, 0 };
	vec3_t b = { 1, 0, 0 };
	vec3_t c = { 0, 1, 0 };
	vec4_t plane;
	qboolean ok = PlaneFromPoints(plane, a, b, c);
	ASSERT(ok, "PlaneFromPoints non-degenerate");
	/* Normal roughly (0,0,1) or (0,0,-1), distance 0 from origin plane z=0 */
	ASSERT_NEAR(fabsf(plane[2]), 1.0f, 0.02f, "PlaneFromPoints |nz|");
	ASSERT_NEAR(plane[3], 0.0f, 0.02f, "PlaneFromPoints dist");

	vec3_t col0 = { 0, 0, 0 };
	vec3_t col1 = { 1, 0, 0 };
	vec3_t col2 = { 2, 0, 0 };
	ok = PlaneFromPoints(plane, col0, col1, col2);
	ASSERT(!ok, "PlaneFromPoints degenerate colinear");
	return 0;
}

static int test_angles(void)
{
	ASSERT_NEAR(AngleNormalize360(370.0f), 10.0f, 0.01f, "AngleNormalize360");
	ASSERT_NEAR(AngleNormalize180(190.0f), -170.0f, 0.01f, "AngleNormalize180");
	ASSERT_NEAR(AngleDelta(10.0f, 350.0f), 20.0f, 0.01f, "AngleDelta wrap");
	return 0;
}

static int test_bounds(void)
{
	vec3_t a1 = { 0, 0, 0 }, a2 = { 10, 10, 10 };
	vec3_t b1 = { 5, 5, 5 }, b2 = { 15, 15, 15 };
	ASSERT(BoundsIntersect(a1, a2, b1, b2), "BoundsIntersect overlap");

	vec3_t c1 = { 100, 0, 0 }, c2 = { 101, 1, 1 };
	ASSERT(!BoundsIntersect(a1, a2, c1, c2), "BoundsIntersect disjoint");

	ASSERT(BoundsIntersectPoint(a1, a2, b1), "BoundsIntersectPoint inside");
	vec3_t outside = { 50, 0, 0 };
	ASSERT(!BoundsIntersectPoint(a1, a2, outside), "BoundsIntersectPoint outside");
	return 0;
}

static int test_dir_byte_roundtrip(void)
{
	vec3_t forward = { 0, 1, 0 };
	int b = DirToByte(forward);
	vec3_t out;
	ByteToDir(b, out);
	ASSERT(VectorLength(out) > 0.99f, "ByteToDir length");
	return 0;
}

static int test_angle_subtract_lerp(void)
{
	ASSERT_NEAR(AngleSubtract(10.0f, 350.0f), 20.0f, 0.01f, "AngleSubtract wrap");
	vec3_t v1 = { 350.0f, 0, 0 }, v2 = { 10.0f, 0, 0 }, v3;
	AnglesSubtract(v1, v2, v3);
	ASSERT_NEAR(v3[0], -20.0f, 0.01f, "AnglesSubtract");

	ASSERT_NEAR(LerpAngle(0.0f, 90.0f, 0.5f), 45.0f, 0.01f, "LerpAngle simple");
	/* 350 -> 10: to is adjusted to 370, midpoint 360 */
	ASSERT_NEAR(LerpAngle(350.0f, 10.0f, 0.5f), 360.0f, 0.5f, "LerpAngle across wrap");
	return 0;
}

static int test_q_crandom_bounds(void)
{
	int s = 42;
	for (int i = 0; i < 20; i++) {
		float x = Q_crandom(&s);
		ASSERT(x >= -1.0f && x <= 1.0f, "Q_crandom range");
	}
	return 0;
}

static int test_plane_signbits_box(void)
{
	cplane_t pl;
	VectorSet(pl.normal, -1.0f, 0.0f, 0.0f);
	pl.dist = 0.0f;
	pl.type = PLANE_X;
	SetPlaneSignbits(&pl);
	ASSERT(pl.signbits == 1, "SetPlaneSignbits x negative");

	vec3_t emins = { -1, -1, -1 };
	vec3_t emaxs = { 1, 1, 1 };
	pl.normal[0] = 1.0f;
	pl.normal[1] = pl.normal[2] = 0.0f;
	pl.dist = 2.0f;
	pl.type = PLANE_X;
	SetPlaneSignbits(&pl);
	/* Plane x=2: box [-1,1] entirely on one side -> 2 */
	ASSERT(BoxOnPlaneSide(emins, emaxs, &pl) == 2, "BoxOnPlaneSide one side X (dist=2)");

	pl.dist = -2.0f;
	ASSERT(BoxOnPlaneSide(emins, emaxs, &pl) == 1, "BoxOnPlaneSide other side X (dist=-2)");

	pl.dist = 0.0f;
	ASSERT(BoxOnPlaneSide(emins, emaxs, &pl) == 3, "BoxOnPlaneSide straddle X (dist=0)");
	return 0;
}

static int test_radius_clear_add_bounds(void)
{
	vec3_t mins = { 0, 0, 0 }, maxs = { 0, 0, 0 };
	ClearBounds(mins, maxs);
	vec3_t p = { 3, 4, 0 };
	AddPointToBounds(p, mins, maxs);
	ASSERT_NEAR(mins[0], 3.0f, 0.001f, "AddPointToBounds min");
	ASSERT_NEAR(maxs[0], 3.0f, 0.001f, "AddPointToBounds max");
	float r = RadiusFromBounds(mins, maxs);
	ASSERT_NEAR(r, 5.0f, 0.02f, "RadiusFromBounds 3-4-0");
	return 0;
}

static int test_matrix_multiply(void)
{
	float a[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	float b[3][3] = { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 0 } };
	float o[3][3];
	MatrixMultiply(a, b, o);
	ASSERT_NEAR(o[0][0], 0.0f, 0.001f, "MatrixMultiply I*B row0");
	ASSERT_NEAR(o[0][1], 1.0f, 0.001f, "MatrixMultiply I*B row0 col1");
	return 0;
}

static int test_plane_type_macro(void)
{
	vec3_t nx = { 1, 0, 0 };
	vec3_t ny = { 0, 1, 0 };
	vec3_t nz = { 0, 0, 1 };
	vec3_t d = { 1, 1, 1 };
	VectorNormalize(d);
	ASSERT(PlaneTypeForNormal(nx) == PLANE_X, "PlaneTypeForNormal X");
	ASSERT(PlaneTypeForNormal(ny) == PLANE_Y, "PlaneTypeForNormal Y");
	ASSERT(PlaneTypeForNormal(nz) == PLANE_Z, "PlaneTypeForNormal Z");
	ASSERT(PlaneTypeForNormal(d) == PLANE_NON_AXIAL, "PlaneTypeForNormal diagonal");
	return 0;
}

int main(void)
{
	if (test_clamp()) return 1;
	if (test_q_rand()) return 1;
	if (test_color_bytes()) return 1;
	if (test_normalize_color()) return 1;
	if (test_plane_from_points()) return 1;
	if (test_angles()) return 1;
	if (test_bounds()) return 1;
	if (test_dir_byte_roundtrip()) return 1;
	if (test_angle_subtract_lerp()) return 1;
	if (test_q_crandom_bounds()) return 1;
	if (test_plane_signbits_box()) return 1;
	if (test_radius_clear_add_bounds()) return 1;
	if (test_matrix_multiply()) return 1;
	if (test_plane_type_macro()) return 1;

	printf("PASS: unit_qmath\n");
	return 0;
}
