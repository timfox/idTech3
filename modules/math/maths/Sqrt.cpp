/* Copyright (C) 2010 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "precompiled.h"

#include "Sqrt.h"

// Based on http://freaknet.org/martin/tape/gos/misc/personal/msc/sqrt/sqrt.html
u32 isqrt64(u64 n)
{
	u64 op = n;
	u64 res = 0;
	u64 one = (u64)1 << 62; // highest power of four <= than the argument

	while (one > op)
		one >>= 2;

	while (one != 0)
	{
		if (op >= res + one)
		{
			op -= (res + one);
			res += (one << 1);
		}
		res >>= 1;
		one >>= 2;
	}
	return (u32)res;
}

// TODO: This should be equivalent to (u32)sqrt((double)n), and in practice
// that seems to be true for all input, so do we actually need this integer-only
// implementation? i.e. are there any platforms / compiler settings where
// sqrt(double) won't give the correct answer? and is sqrt(double) faster?
