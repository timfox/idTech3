/* Copyright (C) 2025 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef INCLUDED_MATH_SQRT
#define INCLUDED_MATH_SQRT

#include "lib/types.h"

/**
 * 64-bit integer square root.
 * Returns r such that r^2 <= n < (r+1)^2, for the complete u64 range.
 */
u32 isqrt64(u64 n);

#endif // INCLUDED_MATH_SQRT
