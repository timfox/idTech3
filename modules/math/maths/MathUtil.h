/* Copyright (C) 2022 Wildfire Games.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef INCLUDED_MATHUTIL
#define INCLUDED_MATHUTIL

#define DEGTORAD(a)					((a) * ((float)M_PI/180.0f))
#define RADTODEG(a)					((a) * (180.0f/(float)M_PI))
#define SQR(x)						((x) * (x))

template<typename T>
inline T Interpolate(const T& a, const T& b, float t)
{
	return a + (b - a) * t;
}

template<typename T>
inline T Clamp(T value, T min, T max)
{
	if (value <= min)
		return min;
	else if (value >= max)
		return max;
	return value;
}

template<typename T>
inline T SmoothStep(T edge0, T edge1, T value)
{
	value = Clamp<T>((value - edge0) / (edge1 - edge0), 0, 1);
	return value * value * (3 - 2 * value);
}

template<typename T>
inline T Sign(const T value)
{
    if (value > T(0))
    	return T(1);
    if (value < T(0))
    	return T(-1);
    return T(0);
}

#endif // INCLUDED_MATHUTIL
