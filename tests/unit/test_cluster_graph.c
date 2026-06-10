/*
 * Unit tests: BSP cluster portal graph + k-hop BFS.
 */
#include <stdio.h>
#include <stdlib.h>

#include "qcommon/cluster_graph.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_line_graph( void )
{
	static const int pairs[] = { 0, 1, 1, 2, 2, 3 };

	ClusterGraph_ResetForTest();
	ClusterGraph_BuildFromTestEdges( 4, pairs, 3 );
	ASSERT( ClusterGraph_GetClusterCount() == 4, "cluster count" );
	ASSERT( ClusterGraph_GetEdgeCount() == 6, "undirected edge count" );

	ASSERT( ClusterGraph_RunBfsTest( 0, 2 ), "bfs run" );
	ASSERT( ClusterGraph_GetHopDistance( 0 ) == 0, "source hop 0" );
	ASSERT( ClusterGraph_GetHopDistance( 2 ) == 2, "two hops" );
	ASSERT( ClusterGraph_GetHopDistance( 3 ) == -1, "beyond max hops" );
	ASSERT( ClusterGraph_IsReachable( 1 ), "mid reachable" );
	ASSERT( !ClusterGraph_IsReachable( 3 ), "far not reachable" );
	ASSERT( ClusterGraph_GetInfluence( 0 ) > 0.99f, "source influence" );
	ASSERT( ClusterGraph_GetInfluence( 3 ) == 0.0f, "unreachable influence" );
	return 0;
}

static int test_star_graph( void )
{
	static const int pairs[] = { 0, 1, 0, 2, 0, 3 };

	ClusterGraph_ResetForTest();
	ClusterGraph_BuildFromTestEdges( 4, pairs, 3 );
	ASSERT( ClusterGraph_RunBfsTest( 0, 1 ), "star bfs" );
	ASSERT( ClusterGraph_GetHopDistance( 2 ) == 1, "spoke hop 1" );
	ASSERT( ClusterGraph_GetHopDistance( 0 ) == 0, "hub hop 0" );
	return 0;
}

int main( int argc, char **argv )
{
	(void)argc;
	(void)argv;

	ClusterGraph_Init();

	if ( test_line_graph() ) return 1;
	if ( test_star_graph() ) return 1;

	printf( "OK: cluster_graph unit tests passed\n" );
	return 0;
}
