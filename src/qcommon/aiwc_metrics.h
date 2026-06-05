#ifndef AIWC_METRICS_H
#define AIWC_METRICS_H

#include "q_shared.h"

#define AIWC_BITS_LEVELS 11 /* n = 0..10 bits dropped (Chilukuri et al. IWOCL'20) */

typedef struct {
	uint64_t virt_addr;
	qboolean is_local;
} aiwc_mem_access_t;

typedef struct aiwc_recorder_s aiwc_recorder_t;

typedef struct {
	uint32_t total_footprint;
	uint32_t footprint_90pct;
	double global_mae;
	double local_mae;
	double lmae[AIWC_BITS_LEVELS];
	double relative_local_usage;
	double psl[AIWC_BITS_LEVELS];
	uint64_t total_accesses;
} aiwc_metrics_t;

aiwc_recorder_t *AIWC_RecorderCreate( void );
void AIWC_RecorderDestroy( aiwc_recorder_t *rec );

void AIWC_RecorderBeginWorkGroup( aiwc_recorder_t *rec, uint32_t wg_id );
void AIWC_RecorderBeginTimestep( aiwc_recorder_t *rec );
void AIWC_RecorderRecord( aiwc_recorder_t *rec, uint64_t virt_addr, qboolean is_local );
void AIWC_RecorderEndWorkGroup( aiwc_recorder_t *rec );
void AIWC_RecorderFinalize( aiwc_recorder_t *rec, aiwc_metrics_t *out );

double AIWC_EntropyBitsDropped( const uint64_t *addrs, uint32_t count, int bits_drop );
void AIWC_MetricsClear( aiwc_metrics_t *m );

#endif /* AIWC_METRICS_H */
