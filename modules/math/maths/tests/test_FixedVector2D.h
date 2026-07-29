/* Copyright (C) 2025 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "lib/self_test.h"

#include "lib/types.h"
#include "maths/Fixed.h"
#include "maths/FixedVector2D.h"

#include <cmath>

#define TS_ASSERT_VEC_EQUALS(v, x, y) \
	TS_ASSERT_EQUALS(v.X.ToDouble(), x); \
	TS_ASSERT_EQUALS(v.Y.ToDouble(), y);

#define TS_ASSERT_VEC_DELTA(v, x, y, delta) \
	TS_ASSERT_DELTA(v.X.ToDouble(), x, delta); \
	TS_ASSERT_DELTA(v.Y.ToDouble(), y, delta);

class TestFixedVector2D : public CxxTest::TestSuite
{
public:
	void test_basic()
	{
		CFixedVector2D v1 (fixed::FromInt(1), fixed::FromInt(2));
		CFixedVector2D v2 (fixed::FromInt(10), fixed::FromInt(20));
		CFixedVector2D v3;
		TS_ASSERT_VEC_EQUALS(v1, 1, 2);
		TS_ASSERT_VEC_EQUALS(v2, 10, 20);
		TS_ASSERT_VEC_EQUALS(v3, 0, 0);
		v3 = v1 + v2;
		TS_ASSERT_VEC_EQUALS(v3, 11, 22);
		v3 += v2;
		TS_ASSERT_VEC_EQUALS(v3, 21, 42);
		v3 -= v2;
		TS_ASSERT_VEC_EQUALS(v3, 11, 22);
		v3 = v1 - v2;
		TS_ASSERT_VEC_EQUALS(v3, -9, -18);
		v3 = -v3;
		TS_ASSERT_VEC_EQUALS(v3, 9, 18);
	}

	void test_Length()
	{
		CFixedVector2D v1 (fixed::FromInt(3), fixed::FromInt(4));
		TS_ASSERT_EQUALS(v1.Length().ToDouble(), 5.0);

		fixed max;
		max.SetInternalValue((i32)0x7fffffff);
		CFixedVector2D v2 (max, fixed::FromInt(0));
		TS_ASSERT_EQUALS(v2.Length().ToDouble(), max.ToDouble());

		fixed large;
		large.SetInternalValue((i32)((double)0x7fffffff/sqrt(2.0))); // largest value that shouldn't cause overflow
		CFixedVector2D v3 (large, large);
		TS_ASSERT_DELTA(v3.Length().ToDouble(), sqrt(2.0)*large.ToDouble(), 0.01);
	}

	void test_CompareLength()
	{
		CFixedVector2D v1(fixed::FromInt(3), fixed::FromInt(4));
		TS_ASSERT_EQUALS(v1.CompareLength(fixed::FromInt(4)), 1);
		TS_ASSERT_EQUALS(v1.CompareLength(fixed::FromInt(5)), 0);
		TS_ASSERT_EQUALS(v1.CompareLength(fixed::FromInt(6)), -1);

		CFixedVector2D v2(fixed::FromInt(2), fixed::FromInt(3));
		CFixedVector2D v3(fixed::FromInt(4), fixed::FromInt(5));
		TS_ASSERT_EQUALS(v1.CompareLength(v2), 1);
		TS_ASSERT_EQUALS(v1.CompareLength(v1), 0);
		TS_ASSERT_EQUALS(v1.CompareLength(v3), -1);

		TS_ASSERT_EQUALS(v1.CompareLengthSquared(SQUARE_U64_FIXED(fixed::FromDouble(4.00))), 1);
		TS_ASSERT_EQUALS(v1.CompareLengthSquared(SQUARE_U64_FIXED(fixed::FromDouble(4.99))), 1);
		TS_ASSERT_EQUALS(v1.CompareLengthSquared(SQUARE_U64_FIXED(fixed::FromDouble(5.00))), 0);
		TS_ASSERT_EQUALS(v1.CompareLengthSquared(SQUARE_U64_FIXED(fixed::FromDouble(5.01))), -1);
		TS_ASSERT_EQUALS(v1.CompareLengthSquared(SQUARE_U64_FIXED(fixed::FromDouble(6.00))), -1);
	}

	void test_Normalize()
	{
		CFixedVector2D v0 (fixed::FromInt(0), fixed::FromInt(0));
		v0.Normalize();
		TS_ASSERT_VEC_EQUALS(v0, 0.0, 0.0);

		CFixedVector2D v1 (fixed::FromInt(3), fixed::FromInt(4));
		v1.Normalize();
		TS_ASSERT_VEC_DELTA(v1, 3.0/5.0, 4.0/5.0, 0.01);

		fixed max;
		max.SetInternalValue((i32)0x7fffffff);
		CFixedVector2D v2 (max, fixed::FromInt(0));
		v2.Normalize();
		TS_ASSERT_VEC_EQUALS(v2, 1.0, 0.0);

		fixed large;
		large.SetInternalValue((i32)((double)0x7fffffff/sqrt(2.0))); // largest value that shouldn't cause overflow
		CFixedVector2D v3 (large, large);
		v3.Normalize();
		TS_ASSERT_VEC_DELTA(v3, 1.0/sqrt(2.0), 1.0/sqrt(2.0), 0.01);
	}

	void test_NormalizeTo()
	{
		{
			CFixedVector2D v (fixed::FromInt(0), fixed::FromInt(0));
			v.Normalize(fixed::FromInt(1));
			TS_ASSERT_VEC_EQUALS(v, 0.0, 0.0);
		}

		{
			CFixedVector2D v (fixed::FromInt(3), fixed::FromInt(4));
			v.Normalize(fixed::FromInt(0));
			TS_ASSERT_VEC_EQUALS(v, 0.0, 0.0);
		}

		{
			CFixedVector2D v (fixed::FromInt(3), fixed::FromInt(4));
			v.Normalize(fixed::FromInt(1));
			TS_ASSERT_VEC_DELTA(v, 3.0/5.0, 4.0/5.0, 0.01);
		}

		{
			CFixedVector2D v (fixed::FromInt(3000), fixed::FromInt(4000));
			v.Normalize(fixed::FromInt(1));
			TS_ASSERT_VEC_DELTA(v, 3.0/5.0, 4.0/5.0, 0.01);
		}

		{
			CFixedVector2D v (fixed::FromInt(3), fixed::FromInt(4));
			v.Normalize(fixed::FromInt(100));
			TS_ASSERT_VEC_DELTA(v, 300.0/5.0, 400.0/5.0, 0.02);
		}

		{
			CFixedVector2D v (fixed::FromInt(3), fixed::FromInt(4));
			v.Normalize(fixed::FromInt(1)/100);
			TS_ASSERT_VEC_DELTA(v, 3.0/500.0, 4.0/500.0, 0.0001);
		}

		{
			CFixedVector2D v (fixed::FromInt(3), fixed::FromInt(4));
			v.Normalize(fixed::FromInt(1)/10000);
			TS_ASSERT_VEC_DELTA(v, 3.0/50000.0, 4.0/50000.0, 0.0001);
		}
	}

	void test_Dot()
	{
		CFixedVector2D v1 (fixed::FromInt(5), fixed::FromInt(6));
		CFixedVector2D v2 (fixed::FromInt(8), fixed::FromInt(-9));
		TS_ASSERT_EQUALS(v1.Dot(v2).ToDouble(), 5*8 + 6*-9);
	}
};
