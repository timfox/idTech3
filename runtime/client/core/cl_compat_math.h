#ifndef CL_COMPAT_MATH_H
#define CL_COMPAT_MATH_H

#include "../../qcommon/q_shared.h"

size_t CL_BuildFallbackGameVersion( const char *gamename, const char *defaultGamename,
	char *out, size_t outSize );
qboolean CL_PointInsideAABB( const vec3_t point, const vec3_t mins, const vec3_t maxs );

#endif
