/*
 * Unit tests: AIWC metrics + matmul parallel spatial locality (IWOCL'20).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "qcommon/aiwc_metrics.h"
#include "qcommon/aiwc_matmul.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static aiwc_metrics_t AIWC_RunVariant( aiwc_matmul_variant_t variant, int N )
{
	aiwc_recorder_t *rec;
	aiwc_metrics_t m;

	memset( &m, 0, sizeof( m ) );
	rec = AIWC_RecorderCreate();
	if ( !rec ) {
		return m;
	}
	AIWC_SimulateMatmul( variant, N, rec );
	AIWC_RecorderFinalize( rec, &m );
	AIWC_RecorderDestroy( rec );
	return m;
}

int main(void)
{
	uint64_t addrs[4];
	double e0;
	double e2;
	aiwc_metrics_t simple;
	aiwc_metrics_t coalA;
	aiwc_metrics_t coalAB;
	aiwc_metrics_t coalABT;

	addrs[0] = 0x1000;
	addrs[1] = 0x1004;
	addrs[2] = 0x1008;
	addrs[3] = 0x100C;
	e0 = AIWC_EntropyBitsDropped( addrs, 4, 0 );
	e2 = AIWC_EntropyBitsDropped( addrs, 4, 2 );
	ASSERT( e2 <= e0 + 1e-6, "entropy drops when bits skipped on clustered addrs" );

	simple = AIWC_RunVariant( AIWC_MATMUL_SIMPLE, 256 );
	coalA = AIWC_RunVariant( AIWC_MATMUL_COALESCED_A, 256 );
	coalAB = AIWC_RunVariant( AIWC_MATMUL_COALESCED_AB, 256 );
	coalABT = AIWC_RunVariant( AIWC_MATMUL_COALESCED_ABT, 256 );

	ASSERT( simple.total_accesses > 0, "simple has accesses" );
	ASSERT( coalAB.relative_local_usage > simple.relative_local_usage,
		"coalescedAB uses more local memory than simple" );
	ASSERT( coalA.relative_local_usage > 0.0, "coalescedA uses local memory" );

	ASSERT( coalABT.psl[10] < simple.psl[10],
		"coalescedABT has lower PSL at 10 bits than simple" );
	ASSERT( coalABT.psl[3] < simple.psl[3] + 0.5,
		"coalescedABT improves mid-range PSL vs simple" );

	printf( "unit_aiwc: ok (simple psl10=%.2f coalescedABT psl10=%.2f rel_local AB=%.2f)\n",
		simple.psl[10], coalABT.psl[10], coalAB.relative_local_usage );
	return 0;
}
