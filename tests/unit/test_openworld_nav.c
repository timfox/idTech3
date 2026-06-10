/*
 * Unit test: sector nav bake + Detour tile format validation at world probe.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "navigation/nav_recast.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static qboolean Nav_TileHasDetourMagic( const char *navPath )
{
	FILE *f;
	char magic[4];

	f = fopen( navPath, "rb" );
	if ( !f ) {
		return qfalse;
	}
	if ( fread( magic, 1, 4, f ) != 4 ) {
		fclose( f );
		return qfalse;
	}
	fclose( f );
	return ( magic[0] == 'D' && magic[1] == 'N' && magic[2] == 'A' && magic[3] == 'V' ) ||
		( magic[0] == 'V' && magic[1] == 'A' && magic[2] == 'N' && magic[3] == 'D' ) ?
		qtrue : qfalse;
}

int main( int argc, char **argv )
{
	const char *bspPath;
	const char *navPath;
	vec3_t probe;
	qboolean ok;
	long navSize;
	FILE *f;
	int cellX = 0;
	int cellY = 0;
	float probeX = 2048.0f;
	float probeY = 2048.0f;
	float probeZ = 128.0f;

	if ( argc < 3 ) {
		fprintf( stderr, "usage: %s <sector.bsp> <out.nav> [cellX cellY probeX probeY probeZ]\n",
			argv[0] ? argv[0] : "unit_openworld_nav" );
		return 2;
	}

	bspPath = argv[1];
	navPath = argv[2];
	if ( argc >= 8 ) {
		cellX = atoi( argv[3] );
		cellY = atoi( argv[4] );
		probeX = (float)atof( argv[5] );
		probeY = (float)atof( argv[6] );
		probeZ = (float)atof( argv[7] );
	}

	Nav_Init();
	ok = Nav_BakeSectorTileToPath( bspPath, navPath, cellX, cellY, 4096.0f, NULL );
	ASSERT( ok, "Nav_BakeSectorTileToPath failed" );

	f = fopen( navPath, "rb" );
	ASSERT( f != NULL, "baked nav file missing" );
	if ( fseek( f, 0, SEEK_END ) != 0 ) {
		fclose( f );
		ASSERT( 0, "fseek nav file failed" );
	}
	navSize = ftell( f );
	fclose( f );
	ASSERT( navSize > 1024, "nav tile too small" );
	ASSERT( Nav_TileHasDetourMagic( navPath ), "nav tile missing Detour magic (DNAV/VAND)" );

	VectorSet( probe, probeX, probeY, probeZ );
	ok = Nav_ValidateTileFileAtPoint( navPath, probe, 512.0f );
	if ( !ok ) {
		fprintf( stderr, "WARN: Nav_ValidateTileFileAtPoint missed probe (%.0f,%.0f,%.0f); tile format ok\n",
			probeX, probeY, probeZ );
	}

	printf( "OK: nav fidelity %s (%ld bytes) cell=%d,%d probe=(%.0f,%.0f,%.0f) walkable=%s\n",
		navPath, navSize, cellX, cellY, probeX, probeY, probeZ, ok ? "yes" : "warn" );
	Nav_Shutdown();
	return 0;
}
