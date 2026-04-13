/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.
===========================================================================
*/
#include "q_shared.h"

/* Declarations also appear in cm_local.h for full collision builds */
qboolean CM_BoundsIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t mins2, const vec3_t maxs2 );
qboolean CM_BoundsIntersectPoint( const vec3_t mins, const vec3_t maxs, const vec3_t point );

#define BOUNDS_CLIP_EPSILON 0.25f /* match cm_test.c: SIMD / float tolerance */

/*
====================
CM_BoundsIntersect
====================
*/
qboolean CM_BoundsIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t mins2, const vec3_t maxs2 )
{
	if (maxs[0] < mins2[0] - BOUNDS_CLIP_EPSILON ||
		maxs[1] < mins2[1] - BOUNDS_CLIP_EPSILON ||
		maxs[2] < mins2[2] - BOUNDS_CLIP_EPSILON ||
		mins[0] > maxs2[0] + BOUNDS_CLIP_EPSILON ||
		mins[1] > maxs2[1] + BOUNDS_CLIP_EPSILON ||
		mins[2] > maxs2[2] + BOUNDS_CLIP_EPSILON)
	{
		return qfalse;
	}

	return qtrue;
}

/*
====================
CM_BoundsIntersectPoint
====================
*/
qboolean CM_BoundsIntersectPoint( const vec3_t mins, const vec3_t maxs, const vec3_t point )
{
	if (maxs[0] < point[0] - BOUNDS_CLIP_EPSILON ||
		maxs[1] < point[1] - BOUNDS_CLIP_EPSILON ||
		maxs[2] < point[2] - BOUNDS_CLIP_EPSILON ||
		mins[0] > point[0] + BOUNDS_CLIP_EPSILON ||
		mins[1] > point[1] + BOUNDS_CLIP_EPSILON ||
		mins[2] > point[2] + BOUNDS_CLIP_EPSILON)
	{
		return qfalse;
	}

	return qtrue;
}
