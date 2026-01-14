/*
=============================================================================
Optimized Math Functions for Vulkan Renderer
=============================================================================
*/

#ifndef TR_MATH_OPTIMIZED_H
#define TR_MATH_OPTIMIZED_H

#include "tr_local.h"

/*
=============================================================================
Fast Square Root Approximations
=============================================================================
*/

// Fast inverse square root using Quake III's famous algorithm
// Accurate to ~0.2% error, much faster than 1.0f / sqrtf()
static inline float FastInvSqrt( float x )
{
	union {
		float f;
		uint32_t i;
	} conv;
	
	conv.f = x;
	conv.i = 0x5f3759df - ( conv.i >> 1 );
	conv.f *= 1.5f - ( x * 0.5f * conv.f * conv.f );
	
	return conv.f;
}

// Fast square root using inverse square root
static inline float FastSqrt( float x )
{
	return x * FastInvSqrt( x );
}

// More accurate fast sqrt (2 Newton-Raphson iterations)
static inline float FastSqrtAccurate( float x )
{
	if ( x == 0.0f ) {
		return 0.0f;
	}
	
	union {
		float f;
		uint32_t i;
	} conv;
	
	conv.f = x;
	conv.i = 0x5f3759df - ( conv.i >> 1 );
	float y = conv.f;
	y *= 1.5f - ( x * 0.5f * y * y );
	y *= 1.5f - ( x * 0.5f * y * y );
	
	return x * y;
}

/*
=============================================================================
Fast Trigonometric Approximations
=============================================================================
*/

// Fast tan approximation using polynomial (accurate for small angles)
// For frustum calculations where angles are typically < 90 degrees
static inline float FastTan( float x )
{
	// Clamp to reasonable range to avoid precision issues
	if ( x > 1.5f ) {
		return tanf( x ); // Fall back to standard tan for large angles
	}
	if ( x < -1.5f ) {
		return tanf( x );
	}
	
	// Polynomial approximation: tan(x) ≈ x + x³/3 + 2x⁵/15
	float x2 = x * x;
	float x3 = x2 * x;
	float x5 = x3 * x2;
	return x + x3 * 0.33333333f + x5 * 0.13333333f;
}

/*
=============================================================================
Optimized Interpolation Functions
=============================================================================
*/

// Fast linear interpolation (same as macro but as function for better optimization)
static inline float FastLerp( float a, float b, float t )
{
	return a + t * ( b - a );
}

// Smoothstep interpolation (better than manual t*t*(3-2*t) in some cases)
static inline float SmoothStep( float edge0, float edge1, float x )
{
	float t = ( x - edge0 ) / ( edge1 - edge0 );
	if ( t < 0.0f ) t = 0.0f;
	if ( t > 1.0f ) t = 1.0f;
	return t * t * ( 3.0f - 2.0f * t );
}

// Smootherstep (even smoother interpolation)
static inline float SmootherStep( float edge0, float edge1, float x )
{
	float t = ( x - edge0 ) / ( edge1 - edge0 );
	if ( t < 0.0f ) t = 0.0f;
	if ( t > 1.0f ) t = 1.0f;
	return t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f );
}

/*
=============================================================================
Optimized Matrix Operations
=============================================================================
*/

// Optimized 4x4 matrix multiplication with better cache locality
// Unrolls inner loop for better performance
static inline void Matrix16MultiplyOptimized( const mat4_t a, const mat4_t b, mat4_t out )
{
	// Unrolled for better performance
	out[ 0] = a[ 0] * b[ 0] + a[ 4] * b[ 1] + a[ 8] * b[ 2] + a[12] * b[ 3];
	out[ 1] = a[ 1] * b[ 0] + a[ 5] * b[ 1] + a[ 9] * b[ 2] + a[13] * b[ 3];
	out[ 2] = a[ 2] * b[ 0] + a[ 6] * b[ 1] + a[10] * b[ 2] + a[14] * b[ 3];
	out[ 3] = a[ 3] * b[ 0] + a[ 7] * b[ 1] + a[11] * b[ 2] + a[15] * b[ 3];
	
	out[ 4] = a[ 0] * b[ 4] + a[ 4] * b[ 5] + a[ 8] * b[ 6] + a[12] * b[ 7];
	out[ 5] = a[ 1] * b[ 4] + a[ 5] * b[ 5] + a[ 9] * b[ 6] + a[13] * b[ 7];
	out[ 6] = a[ 2] * b[ 4] + a[ 6] * b[ 5] + a[10] * b[ 6] + a[14] * b[ 7];
	out[ 7] = a[ 3] * b[ 4] + a[ 7] * b[ 5] + a[11] * b[ 6] + a[15] * b[ 7];
	
	out[ 8] = a[ 0] * b[ 8] + a[ 4] * b[ 9] + a[ 8] * b[10] + a[12] * b[11];
	out[ 9] = a[ 1] * b[ 8] + a[ 5] * b[ 9] + a[ 9] * b[10] + a[13] * b[11];
	out[10] = a[ 2] * b[ 8] + a[ 6] * b[ 9] + a[10] * b[10] + a[14] * b[11];
	out[11] = a[ 3] * b[ 8] + a[ 7] * b[ 9] + a[11] * b[10] + a[15] * b[11];
	
	out[12] = a[ 0] * b[12] + a[ 4] * b[13] + a[ 8] * b[14] + a[12] * b[15];
	out[13] = a[ 1] * b[12] + a[ 5] * b[13] + a[ 9] * b[14] + a[13] * b[15];
	out[14] = a[ 2] * b[12] + a[ 6] * b[13] + a[10] * b[14] + a[14] * b[15];
	out[15] = a[ 3] * b[12] + a[ 7] * b[13] + a[11] * b[14] + a[15] * b[15];
}

// Optimized matrix inversion using block-wise method for better numerical stability
// This is faster and more stable than cofactor expansion for typical transform matrices
#ifdef __cplusplus
extern "C" {
#endif
void Matrix16InverseOptimized( const mat4_t in, mat4_t out );
#ifdef __cplusplus
}
#endif

/*
=============================================================================
Vector Math Optimizations
=============================================================================
*/

// Fast distance squared (avoids sqrt)
static inline float VectorDistanceSqFast( const vec3_t v1, const vec3_t v2 )
{
	vec3_t diff;
	VectorSubtract( v1, v2, diff );
	return DotProduct( diff, diff );
}

// Note: VectorNormalizeFast already exists in q_shared.h
// Use FastInvSqrt for custom normalization if needed

#endif // TR_MATH_OPTIMIZED_H

