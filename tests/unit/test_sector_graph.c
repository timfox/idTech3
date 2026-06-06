/*
 * Unit tests: open-world sector graph CSR + k-hop BFS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "world/sector_graph.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_grid_bfs( void )
{
	int sources[1];
	qboolean ok;
	vec3_t origin;

	SectorGraph_ResetForTest();
	SectorGraph_EnableForTest( qtrue );
	SectorGraph_BuildWindowForTest( 0, 0, 2, qfalse );

	sources[0] = SectorGraph_CellToNode( 0, 0 );
	ASSERT( sources[0] >= 0, "source node id" );
	ok = SectorGraph_RunBfsCpuSources( sources, 1, 2 );
	ASSERT( ok, "bfs run" );
	ASSERT( SectorGraph_IsReachable( 0, 0 ), "source reachable" );
	ASSERT( SectorGraph_IsReachable( 1, 0 ), "neighbor reachable within hops" );
	ASSERT( !SectorGraph_IsReachable( 3, 0 ), "too far for 2 hops from origin only cardinal" );

	VectorSet( origin, 0.5f * 4096.0f, 0.5f * 4096.0f, 0.0f );
	SectorGraph_UpdateReachability( origin, NULL, 0, 4096.0f, 4096.0f * 3.0f, 4 );
	ASSERT( SectorGraph_IsReachable( 0, 0 ), "update reachability source" );
	return 0;
}

static int test_event_to_base( void )
{
	qboolean ok;

	SectorGraph_EnableForTest( qtrue );
	ok = SectorGraph_ReachTest( 0, 0, 2, 0, 4 );
	ASSERT( ok, "horizontal reach within hops" );
	ok = SectorGraph_ReachTest( 0, 0, 0, 3, 2 );
	ASSERT( !ok, "vertical too far for 2 hops cardinal" );
	return 0;
}

static int test_multi_source( void )
{
	vec3_t origins[2];
	int sources[2];

	SectorGraph_ResetForTest();
	SectorGraph_EnableForTest( qtrue );
	SectorGraph_BuildWindowForTest( 0, 0, 3, qfalse );

	VectorSet( origins[0], 0.5f * 4096.0f, 0.5f * 4096.0f, 0.0f );
	VectorSet( origins[1], 2.5f * 4096.0f, 0.5f * 4096.0f, 0.0f );
	SectorGraph_UpdateReachability( origins[0], origins, 2, 4096.0f, 4096.0f * 4.0f, 4 );

	ASSERT( SectorGraph_IsReachable( 0, 0 ), "player A" );
	ASSERT( SectorGraph_IsReachable( 2, 0 ), "player B" );
	ASSERT( SectorGraph_IsReachable( 1, 0 ), "midpoint reachable from union" );

	sources[0] = SectorGraph_CellToNode( 0, 0 );
	sources[1] = SectorGraph_CellToNode( 2, 0 );
	(void)sources;
	return 0;
}

static int test_grid_3x3_component( void )
{
	int sources[1];
	int x, y;
	qboolean ok;

	SectorGraph_ResetForTest();
	SectorGraph_EnableForTest( qtrue );
	SectorGraph_BuildWindowForTest( 1, 1, 1, qfalse );

	sources[0] = SectorGraph_CellToNode( 0, 0 );
	ok = SectorGraph_RunBfsCpuSources( sources, 1, 4 );
	ASSERT( ok, "3x3 bfs run" );

	for ( y = 0; y <= 2; y++ ) {
		for ( x = 0; x <= 2; x++ ) {
			ASSERT( SectorGraph_IsReachable( x, y ), "3x3 connected component cell" );
		}
	}
	ASSERT( !SectorGraph_IsReachable( 3, 0 ), "outside 3x3 window" );
	return 0;
}

int main( int argc, char **argv )
{
	(void)argc;
	(void)argv;

	SectorGraph_Init();
#ifdef SECTOR_GRAPH_UNIT_TEST
	SectorGraph_ResetForTest();
#endif

	if ( test_grid_bfs() ) return 1;
	if ( test_event_to_base() ) return 1;
	if ( test_multi_source() ) return 1;
	if ( test_grid_3x3_component() ) return 1;

	printf( "OK: sector_graph unit tests passed\n" );
	return 0;
}
