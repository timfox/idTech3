/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan renderer utility helpers: parsing, matrix math, color normalization.
===========================================================================
*/

#include "tr_local.h"
#include "vk_util.h"

const char *vk_present_mode_string( VkPresentModeKHR mode )
{
	static char buf[32];

	switch ( mode ) {
		case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
		case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX";
		case VK_PRESENT_MODE_FIFO_KHR: return "FIFO";
		case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
		case VK_PRESENT_MODE_FIFO_LATEST_READY_EXT: return "FIFO_LATEST_READY";
		default: Com_sprintf( buf, sizeof( buf ), "mode#%x", mode ); return buf;
	}
}

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

qboolean vk_parse_fog_tint_string( const char *s, vec3_t out )
{
	char buf[128];
	float r, g, b;
	float maxc;
	char *p;

	if ( !s || !s[0] ) {
		return qfalse;
	}

	Q_strncpyz( buf, s, sizeof( buf ) );
	for ( p = buf; *p; ++p ) {
		if ( *p == ',' || *p == ';' || *p == '\t' ) {
			*p = ' ';
		}
	}

	if ( sscanf( buf, "%f %f %f", &r, &g, &b ) != 3 ) {
		return qfalse;
	}

	maxc = MAX( r, MAX( g, b ) );
	if ( maxc > 1.5f ) {
		r *= ( 1.0f / 255.0f );
		g *= ( 1.0f / 255.0f );
		b *= ( 1.0f / 255.0f );
	}

	out[0] = Com_Clamp( 0.0f, 4.0f, r );
	out[1] = Com_Clamp( 0.0f, 4.0f, g );
	out[2] = Com_Clamp( 0.0f, 4.0f, b );

	/* Treat all-zero as "no tint" to avoid accidental full black fog. */
	if ( out[0] <= 0.0001f && out[1] <= 0.0001f && out[2] <= 0.0001f ) {
		return qfalse;
	}

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
