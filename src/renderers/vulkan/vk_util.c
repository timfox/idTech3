/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan renderer utility helpers: parsing, matrix math, color normalization.
===========================================================================
*/

#include "tr_local.h"
#include "vk_util.h"

qboolean vk_parse_rgb_string( const char *s, vec3_t out )
{
	float r, g, b;

	if ( !s || !s[0] ) {
		return qfalse;
	}

	if ( sscanf( s, "%f %f %f", &r, &g, &b ) != 3 ) {
		return qfalse;
	}

	out[0] = r;
	out[1] = g;
	out[2] = b;
	return qtrue;
}

float vk_matrix_max_abs_diff( const float *a, const float *b )
{
	float max_diff = 0.0f;

	for ( int i = 0; i < 16; i++ ) {
		const float d = fabsf( a[i] - b[i] );
		if ( d > max_diff ) {
			max_diff = d;
		}
	}
	return max_diff;
}

void vk_normalize_rgb_luma_safe( vec3_t io )
{
	float maxc = MAX( io[0], MAX( io[1], io[2] ) );

	if ( maxc <= 0.0f ) {
		VectorSet( io, 1.0f, 1.0f, 1.0f );
		return;
	}

	if ( maxc > 1.0f ) {
		VectorScale( io, 1.0f / maxc, io );
	}
}
