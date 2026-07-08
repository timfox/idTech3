#include <stdio.h>

#include "cl_compat_math.h"

size_t CL_BuildFallbackGameVersion( const char *gamename, const char *defaultGamename,
	char *out, size_t outSize ) {
	const char *baseName;
	int written;

	if ( !out || outSize == 0 ) {
		return 0;
	}

	baseName = ( gamename && gamename[0] ) ? gamename : defaultGamename;
	if ( !baseName || !baseName[0] ) {
		baseName = "baseq3";
	}

	written = snprintf( out, outSize, "%s-1", baseName );
	if ( written < 0 ) {
		out[0] = '\0';
		return 0;
	}

	if ( (size_t)written >= outSize ) {
		return outSize - 1;
	}

	return (size_t)written;
}

qboolean CL_PointInsideAABB( const vec3_t point, const vec3_t mins, const vec3_t maxs ) {
	return (qboolean)(
		point[0] >= mins[0] && point[0] <= maxs[0] &&
		point[1] >= mins[1] && point[1] <= maxs[1] &&
		point[2] >= mins[2] && point[2] <= maxs[2] );
}
