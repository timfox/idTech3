/*
 * Unit tests: client compatibility math helpers used by fallback
 * game-version construction and spawn-point AABB validation.
 */
#include <stdio.h>
#include <string.h>

#include "cl_compat_math.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_STREQ(a, b, msg) do { \
	if (strcmp((a), (b)) != 0) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static void v3( vec3_t out, float x, float y, float z ) {
	out[0] = x;
	out[1] = y;
	out[2] = z;
}

static int test_build_fallback_game_version_basic(void) {
	char built[32];

	CL_BuildFallbackGameVersion( "openarena", "baseq3", built, sizeof( built ) );
	ASSERT_STREQ( built, "openarena-1", "openarena fallback version" );

	CL_BuildFallbackGameVersion( "baseoa", "baseq3", built, sizeof( built ) );
	ASSERT_STREQ( built, "baseoa-1", "baseoa fallback version" );
	return 0;
}

static int test_build_fallback_game_version_default(void) {
	char built[32];

	CL_BuildFallbackGameVersion( "", "baseq3", built, sizeof( built ) );
	ASSERT_STREQ( built, "baseq3-1", "empty gamename uses default" );

	CL_BuildFallbackGameVersion( NULL, NULL, built, sizeof( built ) );
	ASSERT_STREQ( built, "baseq3-1", "NULL names use hard default" );
	return 0;
}

static int test_build_fallback_game_version_truncates_safely(void) {
	char built[8];

	CL_BuildFallbackGameVersion( "openarena", "baseq3", built, sizeof( built ) );
	ASSERT_STREQ( built, "openare", "truncated output stays terminated" );
	return 0;
}

static int test_point_inside_aabb_accepts_spawn_inside(void) {
	vec3_t mins, maxs, spawn;

	v3( mins, -1024.0f, -1024.0f, -128.0f );
	v3( maxs, 1024.0f, 1024.0f, 512.0f );
	v3( spawn, 128.0f, -64.0f, 96.0f );
	ASSERT( CL_PointInsideAABB( spawn, mins, maxs ) == qtrue, "spawn inside map AABB" );
	return 0;
}

static int test_point_inside_aabb_accepts_boundary_spawn(void) {
	vec3_t mins, maxs, spawn;

	v3( mins, -256.0f, -256.0f, 0.0f );
	v3( maxs, 256.0f, 256.0f, 192.0f );
	v3( spawn, -256.0f, 32.0f, 48.0f );
	ASSERT( CL_PointInsideAABB( spawn, mins, maxs ) == qtrue, "spawn on boundary stays valid" );
	return 0;
}

static int test_point_inside_aabb_rejects_spawn_outside(void) {
	vec3_t mins, maxs, spawn;

	v3( mins, -512.0f, -512.0f, -64.0f );
	v3( maxs, 512.0f, 512.0f, 256.0f );
	v3( spawn, 700.0f, 0.0f, 64.0f );
	ASSERT( CL_PointInsideAABB( spawn, mins, maxs ) == qfalse, "spawn outside map AABB" );
	return 0;
}

int main( void ) {
	if ( test_build_fallback_game_version_basic() ) return 1;
	if ( test_build_fallback_game_version_default() ) return 1;
	if ( test_build_fallback_game_version_truncates_safely() ) return 1;
	if ( test_point_inside_aabb_accepts_spawn_inside() ) return 1;
	if ( test_point_inside_aabb_accepts_boundary_spawn() ) return 1;
	if ( test_point_inside_aabb_rejects_spawn_outside() ) return 1;
	return 0;
}
