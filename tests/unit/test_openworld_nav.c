/*
 * Unit test: offline sector nav bake (Recast) from sector BSP fixture.
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

int main( int argc, char **argv )
{
	const char *bspPath;
	const char *navPath;
	qboolean ok;
	long navSize;
	FILE *f;

	if ( argc < 3 ) {
		fprintf( stderr, "usage: %s <sector.bsp> <out.nav>\n", argv[0] ? argv[0] : "unit_openworld_nav" );
		return 2;
	}

	bspPath = argv[1];
	navPath = argv[2];

	Nav_Init();
	ok = Nav_BakeSectorTileToPath( bspPath, navPath, 0, 0, 4096.0f, NULL );
	ASSERT( ok, "Nav_BakeSectorTileToPath failed" );

	f = fopen( navPath, "rb" );
	ASSERT( f != NULL, "baked nav file missing" );
	if ( fseek( f, 0, SEEK_END ) != 0 ) {
		fclose( f );
		ASSERT( 0, "fseek nav file failed" );
	}
	navSize = ftell( f );
	fclose( f );
	ASSERT( navSize > 64, "nav tile too small" );

	printf( "OK: baked nav %s (%ld bytes) from %s\n", navPath, navSize, bspPath );
	Nav_Shutdown();
	return 0;
}
