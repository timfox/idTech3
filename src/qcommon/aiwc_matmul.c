/*
===========================================================================
OpenCL-style matrix multiply memory-access simulators for AIWC validation.
Kernels follow Chilukuri et al. IWOCL'20 appendices (simple / coalesced*).
===========================================================================
*/

#include "aiwc_matmul.h"

#define AIWC_TILE_DIM 16

#define AIWC_ADDR_A 0x00100000ULL
#define AIWC_ADDR_B 0x00200000ULL
#define AIWC_ADDR_C 0x00300000ULL
#define AIWC_ADDR_LOCAL_BASE 0x01000000ULL

static uint64_t AIWC_GlobalAddr( uint64_t base, int index )
{
	return base + (uint64_t)index * 4ULL;
}

static uint64_t AIWC_LocalAddr( uint32_t wg_id, int slot )
{
	return AIWC_ADDR_LOCAL_BASE + (uint64_t)wg_id * 8192ULL + (uint64_t)slot * 4ULL;
}

static void AIWC_RecordGlobal( aiwc_recorder_t *rec, uint64_t base, int index )
{
	AIWC_RecorderRecord( rec, AIWC_GlobalAddr( base, index ), qfalse );
}

static void AIWC_RecordLocal( aiwc_recorder_t *rec, uint32_t wg_id, int slot )
{
	AIWC_RecorderRecord( rec, AIWC_LocalAddr( wg_id, slot ), qtrue );
}

const char *AIWC_MatmulVariantName( aiwc_matmul_variant_t variant )
{
	switch ( variant ) {
	case AIWC_MATMUL_SIMPLE:
		return "simple";
	case AIWC_MATMUL_COALESCED_A:
		return "coalescedA";
	case AIWC_MATMUL_COALESCED_AB:
		return "coalescedAB";
	case AIWC_MATMUL_COALESCED_ABT:
		return "coalescedABT";
	case AIWC_MATMUL_ALIGNED_ABT:
		return "alignedABT";
	default:
		return "unknown";
	}
}

static void AIWC_SimSimple( int N, aiwc_recorder_t *rec )
{
	int wg_row;
	int wg_col;
	int num_wg = N / AIWC_TILE_DIM;

	for ( wg_row = 0; wg_row < num_wg; wg_row++ ) {
		for ( wg_col = 0; wg_col < num_wg; wg_col++ ) {
			int k;
			int localRow;
			int localCol;
			uint32_t wg_id = (uint32_t)( wg_row * num_wg + wg_col );

			AIWC_RecorderBeginWorkGroup( rec, wg_id );

			for ( k = 0; k < N; k++ ) {
				AIWC_RecorderBeginTimestep( rec );
				for ( localRow = 0; localRow < AIWC_TILE_DIM; localRow++ ) {
					int globalRow = wg_row * AIWC_TILE_DIM + localRow;
					for ( localCol = 0; localCol < AIWC_TILE_DIM; localCol++ ) {
						int globalCol = wg_col * AIWC_TILE_DIM + localCol;
						AIWC_RecordGlobal( rec, AIWC_ADDR_A, globalRow * N + k );
						AIWC_RecordGlobal( rec, AIWC_ADDR_B, k * N + globalCol );
					}
				}
			}

			AIWC_RecorderBeginTimestep( rec );
			for ( localRow = 0; localRow < AIWC_TILE_DIM; localRow++ ) {
				int globalRow = wg_row * AIWC_TILE_DIM + localRow;
				for ( localCol = 0; localCol < AIWC_TILE_DIM; localCol++ ) {
					int globalCol = wg_col * AIWC_TILE_DIM + localCol;
					AIWC_RecordGlobal( rec, AIWC_ADDR_C, globalRow * N + globalCol );
				}
			}

			AIWC_RecorderEndWorkGroup( rec );
		}
	}
}

static void AIWC_SimCoalescedA( int N, aiwc_recorder_t *rec )
{
	int wg_row;
	int wg_col;
	int num_wg = N / AIWC_TILE_DIM;

	for ( wg_row = 0; wg_row < num_wg; wg_row++ ) {
		for ( wg_col = 0; wg_col < num_wg; wg_col++ ) {
			int i;
			int k;
			int localRow;
			int localCol;
			uint32_t wg_id = (uint32_t)( wg_row * num_wg + wg_col );

			AIWC_RecorderBeginWorkGroup( rec, wg_id );

			for ( i = 0; i < num_wg; i++ ) {
				AIWC_RecorderBeginTimestep( rec );
				for ( localRow = 0; localRow < AIWC_TILE_DIM; localRow++ ) {
					int globalRow = wg_row * AIWC_TILE_DIM + localRow;
					for ( localCol = 0; localCol < AIWC_TILE_DIM; localCol++ ) {
						int tiledRow = globalRow * N + localCol;
						AIWC_RecordGlobal( rec, AIWC_ADDR_A, tiledRow + i * AIWC_TILE_DIM );
					}
				}

				AIWC_RecorderBeginTimestep( rec );
				for ( localRow = 0; localRow < AIWC_TILE_DIM; localRow++ ) {
					for ( localCol = 0; localCol < AIWC_TILE_DIM; localCol++ ) {
						AIWC_RecordLocal( rec, wg_id, localRow * AIWC_TILE_DIM + localCol );
					}
				}

				for ( k = 0; k < AIWC_TILE_DIM; k++ ) {
					AIWC_RecorderBeginTimestep( rec );
					for ( localRow = 0; localRow < AIWC_TILE_DIM; localRow++ ) {
						for ( localCol = 0; localCol < AIWC_TILE_DIM; localCol++ ) {
							int globalCol = wg_col * AIWC_TILE_DIM + localCol;
							AIWC_RecordLocal( rec, wg_id, localRow * AIWC_TILE_DIM + k );
							AIWC_RecordGlobal( rec, AIWC_ADDR_B, ( i * AIWC_TILE_DIM + k ) * N + globalCol );
						}
					}
				}
			}

			AIWC_RecorderBeginTimestep( rec );
			for ( localRow = 0; localRow < AIWC_TILE_DIM; localRow++ ) {
				int globalRow = wg_row * AIWC_TILE_DIM + localRow;
				for ( localCol = 0; localCol < AIWC_TILE_DIM; localCol++ ) {
					int globalCol = wg_col * AIWC_TILE_DIM + localCol;
					AIWC_RecordGlobal( rec, AIWC_ADDR_C, globalRow * N + globalCol );
				}
			}

			AIWC_RecorderEndWorkGroup( rec );
		}
	}
}

static void AIWC_SimCoalescedAB( int N, aiwc_recorder_t *rec, qboolean transposed_a )
{
	int wg_row;
	int wg_col;
	int num_wg = N / AIWC_TILE_DIM;

	for ( wg_row = 0; wg_row < num_wg; wg_row++ ) {
		for ( wg_col = 0; wg_col < num_wg; wg_col++ ) {
			int i;
			int k;
			int localRow;
			int localCol;
			uint32_t wg_id = (uint32_t)( wg_row * num_wg + wg_col );

			AIWC_RecorderBeginWorkGroup( rec, wg_id );

			for ( i = 0; i < num_wg; i++ ) {
				AIWC_RecorderBeginTimestep( rec );
				for ( localRow = 0; localRow < AIWC_TILE_DIM; localRow++ ) {
					int globalRow = wg_row * AIWC_TILE_DIM + localRow;
					for ( localCol = 0; localCol < AIWC_TILE_DIM; localCol++ ) {
						int globalCol = wg_col * AIWC_TILE_DIM + localCol;
						int tiledRow = globalRow * N + i * AIWC_TILE_DIM + localCol;
						int tiledCol = globalCol + ( AIWC_TILE_DIM * i + localRow ) * N;
						AIWC_RecordGlobal( rec, AIWC_ADDR_A, tiledRow );
						AIWC_RecordGlobal( rec, AIWC_ADDR_B, tiledCol );
					}
				}

				AIWC_RecorderBeginTimestep( rec );
				for ( localRow = 0; localRow < AIWC_TILE_DIM; localRow++ ) {
					for ( localCol = 0; localCol < AIWC_TILE_DIM; localCol++ ) {
						int a_slot = transposed_a ? ( localCol * AIWC_TILE_DIM + localRow ) : ( localRow * AIWC_TILE_DIM + localCol );
						int b_slot = AIWC_TILE_DIM * AIWC_TILE_DIM + localRow * AIWC_TILE_DIM + localCol;
						AIWC_RecordLocal( rec, wg_id, a_slot );
						AIWC_RecordLocal( rec, wg_id, b_slot );
					}
				}

				for ( k = 0; k < AIWC_TILE_DIM; k++ ) {
					AIWC_RecorderBeginTimestep( rec );
					for ( localRow = 0; localRow < AIWC_TILE_DIM; localRow++ ) {
						for ( localCol = 0; localCol < AIWC_TILE_DIM; localCol++ ) {
							int a_slot = localRow * AIWC_TILE_DIM + k;
							int b_slot = AIWC_TILE_DIM * AIWC_TILE_DIM + k * AIWC_TILE_DIM + localCol;
							if ( transposed_a ) {
								a_slot = localCol * AIWC_TILE_DIM + k;
							}
							AIWC_RecordLocal( rec, wg_id, a_slot );
							AIWC_RecordLocal( rec, wg_id, b_slot );
						}
					}
				}
			}

			AIWC_RecorderBeginTimestep( rec );
			for ( localRow = 0; localRow < AIWC_TILE_DIM; localRow++ ) {
				int globalRow = wg_row * AIWC_TILE_DIM + localRow;
				for ( localCol = 0; localCol < AIWC_TILE_DIM; localCol++ ) {
					int globalCol = wg_col * AIWC_TILE_DIM + localCol;
					AIWC_RecordGlobal( rec, AIWC_ADDR_C, globalRow * N + globalCol );
				}
			}

			AIWC_RecorderEndWorkGroup( rec );
		}
	}
}

void AIWC_SimulateMatmul( aiwc_matmul_variant_t variant, int N, aiwc_recorder_t *rec )
{
	if ( !rec || N <= 0 || ( N % AIWC_TILE_DIM ) != 0 ) {
		return;
	}

	switch ( variant ) {
	case AIWC_MATMUL_SIMPLE:
		AIWC_SimSimple( N, rec );
		break;
	case AIWC_MATMUL_COALESCED_A:
		AIWC_SimCoalescedA( N, rec );
		break;
	case AIWC_MATMUL_COALESCED_AB:
		AIWC_SimCoalescedAB( N, rec, qfalse );
		break;
	case AIWC_MATMUL_COALESCED_ABT:
		AIWC_SimCoalescedAB( N, rec, qtrue );
		break;
	case AIWC_MATMUL_ALIGNED_ABT:
		AIWC_SimCoalescedAB( N, rec, qtrue );
		break;
	default:
		break;
	}
}
