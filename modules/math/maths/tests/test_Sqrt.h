/* Copyright (C) 2025 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "lib/self_test.h"

#include "lib/types.h"
#include "maths/Sqrt.h"

#include <cmath>
#include <cstddef>
#include <random>

class TestSqrt : public CxxTest::TestSuite
{
public:
	void t(u32 n)
	{
		TS_ASSERT_EQUALS(isqrt64((u64)n*(u64)n), n);
	}

	void s(u64 n, u64 exp)
	{
		TS_ASSERT_EQUALS((u64)isqrt64(n), exp);
	}

	void test_sqrt()
	{
		t(0);
		t(1);
		t(2);
		t(255);
		t(256);
		t(257);
		t(65535);
		t(65536);
		t(65537);
		t(16777215);
		t(16777216);
		t(16777217);
		t(2147483647);
		t(2147483648u);
		t(2147483649u);
		t(4294967295u);

		s(2, 1);
		s(3, 1);
		s(4, 2);
		s(255, 15);
		s(256, 16);
		s(257, 16);
		s(65535, 255);
		s(65536, 256);
		s(65537, 256);
		s(999999, 999);
		s(1000000, 1000);
		s(1000001, 1000);
		s((u64)-1, 4294967295u);
	}

	void test_random()
	{
		// Test with some random u64s, to make sure the output agrees with floor(sqrt(double))
		// (TODO: This might be making non-portable assumptions about sqrt(double))

		std::mt19937 rng;
		std::uniform_int_distribution<u64> ints(0, (u64)-1);

		for (size_t i = 0; i < 1024; ++i)
		{
			u64 n = ints(rng);
			s(n, static_cast<u64>(sqrt(static_cast<double>(n))));
		}
	}
};
