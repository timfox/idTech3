#ifndef AIWC_MATMUL_H
#define AIWC_MATMUL_H

#include "aiwc_metrics.h"

#define AIWC_MATMUL_TILE_DIM 16

typedef enum {
	AIWC_MATMUL_SIMPLE = 0,
	AIWC_MATMUL_COALESCED_A,
	AIWC_MATMUL_COALESCED_AB,
	AIWC_MATMUL_COALESCED_ABT,
	AIWC_MATMUL_ALIGNED_ABT,
	AIWC_MATMUL_VARIANT_COUNT
} aiwc_matmul_variant_t;

const char *AIWC_MatmulVariantName( aiwc_matmul_variant_t variant );
void AIWC_SimulateMatmul( aiwc_matmul_variant_t variant, int N, aiwc_recorder_t *rec );

#endif /* AIWC_MATMUL_H */
