/*
 * Unit tests: consistent submodular sector residency selector.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "world/world_residency.h"
#include "world/world_proc.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_symmetric_difference(void)
{
	worldResidencyCell_t a[] = { {0, 0}, {1, 0} };
	worldResidencyCell_t b[] = { {1, 0}, {2, 0} };
	int diff;

	diff = WorldResidency_SymmetricDifference( a, 2, b, 2 );
	ASSERT( diff == 2, "sym diff two one-sided cells" );

	diff = WorldResidency_SymmetricDifference( a, 2, a, 2 );
	ASSERT( diff == 0, "sym diff identical" );
	return 0;
}

static int test_select_budget(void)
{
	worldResidencyCandidate_t candidates[16];
	worldResidencyCell_t out[16];
	int i;
	int n;

	for ( i = 0; i < 16; i++ ) {
		candidates[i].cellX = i % 4;
		candidates[i].cellY = i / 4;
		candidates[i].score = 16.0f - (float)i;
		candidates[i].regionId = i & 7;
	}

	n = WorldResidency_SelectCardinality( WR_LAYER_COLLISION, candidates, 16, NULL, 0, out, 16, qfalse );
	ASSERT( n <= 64, "collision budget cap" );
	ASSERT( n >= 8, "select at least some candidates" );
	return 0;
}

static int test_matroid_one_per_region(void)
{
	worldResidencyCandidate_t candidates[8];
	worldResidencyCell_t out[8];
	int i;
	int n;
	int regions[8];
	int r;

	for ( i = 0; i < 8; i++ ) {
		candidates[i].cellX = i;
		candidates[i].cellY = 0;
		candidates[i].regionId = i & 3;
		candidates[i].score = (float)( 10 - i );
	}

	n = WorldResidency_SelectCardinality( WR_LAYER_COLLISION, candidates, 8, NULL, 0, out, 8, qtrue );
	ASSERT( n <= 4, "matroid at most one per region" );

	memset( regions, 0, sizeof( regions ) );
	for ( i = 0; i < n; i++ ) {
		r = WorldProc_RegionAtSector( out[i].cellX, out[i].cellY, 4096.0f );
		regions[r]++;
		ASSERT( regions[r] <= 1, "duplicate region in matroid set" );
	}
	return 0;
}

static int test_score_oracle(void)
{
	vec3_t view;
	float s0, s1;

	VectorSet( view, 2048.0f, 2048.0f, 64.0f );
	s0 = WorldResidency_ScoreCell( WR_LAYER_COLLISION, 0, 0, view, 12288.0f, 4096.0f, qfalse );
	s1 = WorldResidency_ScoreCell( WR_LAYER_COLLISION, 0, 0, view, 12288.0f, 4096.0f, qtrue );
	ASSERT( s0 >= 0.0f, "score non-negative" );
	ASSERT( s1 >= s0, "sticky bonus increases score" );
	return 0;
}

static int test_consistency_stream(void)
{
	worldResidencyCell_t prev[16];
	worldResidencyCell_t next[16];
	worldResidencyCandidate_t candidates[32];
	int prevCount = 0;
	int nextCount;
	int step;
	int maxDiff = 0;
	int diff;

	for ( step = 0; step < 20; step++ ) {
		int i;

		for ( i = 0; i < 32; i++ ) {
			candidates[i].cellX = ( i % 8 ) + step % 2;
			candidates[i].cellY = i / 8;
			candidates[i].regionId = i & 7;
			candidates[i].score = 32.0f - (float)i + (float)( step % 3 );
		}
		nextCount = WorldResidency_SelectCardinality( WR_LAYER_COLLISION, candidates, 32,
			prev, prevCount, next, 16, qfalse );
		diff = WorldResidency_SymmetricDifference( prev, prevCount, next, nextCount );
		if ( diff > maxDiff ) {
			maxDiff = diff;
		}
		prevCount = nextCount;
		memcpy( prev, next, (size_t)prevCount * sizeof( prev[0] ) );
	}
	ASSERT( maxDiff <= 32, "sym diff bounded on synthetic stream" );
	return 0;
}

int main( int argc, char **argv )
{
	(void)argc;
	(void)argv;

	WorldResidency_Init();
#ifdef WORLD_RESIDENCY_UNIT_TEST
	WorldResidency_ResetStateForTest();
#endif

	if ( test_symmetric_difference() ) return 1;
	if ( test_score_oracle() ) return 1;
	if ( test_select_budget() ) return 1;
	if ( test_matroid_one_per_region() ) return 1;
	if ( test_consistency_stream() ) return 1;

	printf( "OK: world_residency unit tests passed\n" );
	return 0;
}
