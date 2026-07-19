/*
===========================================================================
NEBRDF — hypercube neighbors, enhancement order, fit/render timings.
===========================================================================
*/

#include "nebrdf/nebrdf_internal.h"

#include <string.h>

int NeBrdf_EnhancementOrder( int *outNodeIds, int outCap )
{
	int n = NEBRDF_ENHANCE_ORDER_LEN;

	if ( !outNodeIds || outCap <= 0 ) {
		return 0;
	}
	if ( n > outCap ) {
		n = outCap;
	}
	memcpy( outNodeIds, nebrdf_enhance_order, (size_t)n * sizeof( int ) );
	return n;
}

int NeBrdf_HypercubeNeighbors( int nodeCount, int maxHamming )
{
	int N;
	int i;
	long long binom;
	long long total;

	if ( nodeCount <= 0 ) {
		N = NEBRDF_NODE_COUNT;
	} else {
		N = nodeCount;
	}
	if ( maxHamming < 0 ) {
		return 0;
	}
	if ( maxHamming > N ) {
		maxHamming = N;
	}

	/* Sum_{k=0..maxHamming} C(N,k); Hamming ≤1 → N+1. */
	total = 0;
	binom = 1; /* C(N,0) */
	for ( i = 0; i <= maxHamming; i++ ) {
		total += binom;
		if ( i < maxHamming && i < N ) {
			binom = binom * ( N - i ) / ( i + 1 );
		}
	}
	if ( total > 0x7fffffff ) {
		return 0x7fffffff;
	}
	return (int)total;
}

float NeBrdf_FitTimeSec( int enhanced )
{
	/* Paper §7: 27.3s enhanced vs 34.2s GGX for 10^5 measurements. */
	return enhanced ? 27.3f : 34.2f;
}

float NeBrdf_RenderRaysPerSec( int enhanced )
{
	/* Mitsuba rays/s ×10^6: 13.68 enhanced vs 21.83 GGX. */
	return enhanced ? 13.68f : 21.83f;
}
