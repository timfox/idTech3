#pragma once

#include "qcommon/q_shared.h"

/* Tree Traversal Prefetcher (TTP) analytical model — Tozlu et al., arXiv:2605.16253 */

typedef struct {
	uint64_t total_pops;
	uint64_t streak_pops[4]; /* 0=1st pop after push, 1=2nd consecutive pop, ... 3=4th+ */
} ttp_pop_histogram_t;

typedef struct {
	float baseline_miss_rate;
	float ttp_miss_rate;
	float coverage;     /* (baseline - ttp) / baseline */
	float accuracy;     /* useful prefetches / total prefetches */
	float speedup;      /* memory-latency bound model */
	uint64_t prefetches_issued;
	uint64_t prefetches_useful;
} ttp_model_result_t;

typedef enum {
	TTP_TRAVERSAL_DFS = 0,
	TTP_TRAVERSAL_BFS
} ttp_traversal_mode_t;

void TTP_ModelDFS( const ttp_pop_histogram_t *hist, int prefetch_intensity[3], float mem_wait_fraction,
	ttp_model_result_t *out );

void TTP_ModelBFS( const ttp_pop_histogram_t *hist, int prefetch_distance, float mem_wait_fraction,
	ttp_model_result_t *out );

void TTP_DefaultPrefetchIntensity( int out_intensity[3] );
float TTP_BFSCoverageForDistance( int prefetch_distance );
