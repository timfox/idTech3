/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Analytical TTP model (Tozlu et al., arXiv:2605.16253).
===========================================================================
*/

#include "ttp_model.h"

#include <float.h>
#include <math.h>

/* Paper-reported average L1 accuracy for DFS TTP (Fig. 14). */
#define TTP_DFS_L1_ACCURACY_REF 0.9892f

void TTP_DefaultPrefetchIntensity( int out_intensity[3] )
{
	if ( !out_intensity ) {
		return;
	}
	out_intensity[0] = 1;
	out_intensity[1] = 2;
	out_intensity[2] = 16;
}

static float TTP_SpeedupFromCoverage( float coverage, float mem_wait_fraction )
{
	float effective;

	if ( mem_wait_fraction <= 0.0f ) {
		mem_wait_fraction = 0.001f;
	}
	if ( mem_wait_fraction >= 1.0f ) {
		mem_wait_fraction = 0.999f;
	}
	if ( coverage < 0.0f ) {
		coverage = 0.0f;
	}
	if ( coverage > 1.0f ) {
		coverage = 1.0f;
	}

	effective = ( 1.0f - mem_wait_fraction ) + mem_wait_fraction * ( 1.0f - coverage );
	if ( effective <= 0.0f ) {
		return 1.0f;
	}
	return 1.0f / effective;
}

static uint64_t TTP_RoundUpRatio( uint64_t numerator, float ratio )
{
	double denom;

	if ( numerator == 0u ) {
		return 0u;
	}
	denom = ( ratio > FLT_EPSILON ) ? (double)ratio : 1.0;
	return (uint64_t)ceil( (double)numerator / denom );
}

float TTP_BFSCoverageForDistance( int prefetch_distance )
{
	float t;

	if ( prefetch_distance <= 1 ) {
		return 0.65637064f;
	}
	if ( prefetch_distance == 2 ) {
		return 0.73170733f;
	}
	if ( prefetch_distance == 4 ) {
		return 0.77922076f;
	}
	if ( prefetch_distance < 4 ) {
		t = (float)( prefetch_distance - 2 ) * 0.5f;
		return 0.73170733f + t * ( 0.77922076f - 0.73170733f );
	}
	if ( prefetch_distance > 8 ) {
		prefetch_distance = 8;
	}

	/* Diminishing returns beyond the paper's N=4 default. */
	t = (float)( prefetch_distance - 4 ) / 4.0f;
	return 0.77922076f + t * ( 0.795f - 0.77922076f );
}

void TTP_ModelDFS( const ttp_pop_histogram_t *hist, int prefetch_intensity[3], float mem_wait_fraction,
	ttp_model_result_t *out )
{
	uint64_t baseline_misses;
	uint64_t prefetches;
	uint64_t useful;
	uint64_t saved;
	int k;
	float miss_per_pop;

	if ( !out ) {
		return;
	}
	(void)prefetch_intensity;

	out->baseline_miss_rate = 0.0f;
	out->ttp_miss_rate = 0.0f;
	out->coverage = 0.0f;
	out->accuracy = 0.0f;
	out->speedup = 1.0f;
	out->prefetches_issued = 0u;
	out->prefetches_useful = 0u;

	if ( !hist || hist->total_pops == 0u ) {
		return;
	}

	/* Paper Fig. 6: longer pop streaks dominate RT read misses. Model miss weight by streak index. */
	miss_per_pop = 1.0f;
	baseline_misses = 0u;
	prefetches = 0u;
	useful = 0u;
	saved = 0u;

	for ( k = 0; k < 4; k++ ) {
		uint64_t pops = hist->streak_pops[k];
		float weight;
		uint64_t streak_misses;
		uint64_t streak_saved;

		if ( pops == 0u ) {
			continue;
		}

		weight = 1.0f + (float)k * 0.75f;
		streak_misses = (uint64_t)( (float)pops * miss_per_pop * weight );
		baseline_misses += streak_misses;

		if ( k == 0 ) {
			continue;
		}

		/* Each consecutive pop prefetches up to intensity nodes already on the stack (accurate). */
		streak_saved = pops;
		if ( streak_saved > streak_misses ) {
			streak_saved = streak_misses;
		}
		saved += streak_saved;
		useful += streak_saved;
	}

	if ( baseline_misses == 0u ) {
		return;
	}

	out->baseline_miss_rate = (float)baseline_misses / (float)hist->total_pops;
	prefetches = TTP_RoundUpRatio( useful, TTP_DFS_L1_ACCURACY_REF );
	if ( prefetches < useful ) {
		prefetches = useful;
	}
	out->prefetches_issued = prefetches;
	out->prefetches_useful = useful;
	out->coverage = (float)saved / (float)baseline_misses;
	out->accuracy = ( prefetches > 0u ) ? (float)useful / (float)prefetches : 0.0f;
	if ( out->accuracy > 1.0f ) {
		out->accuracy = 1.0f;
	}
	out->ttp_miss_rate = out->baseline_miss_rate * ( 1.0f - out->coverage );
	out->speedup = TTP_SpeedupFromCoverage( out->coverage, mem_wait_fraction );
}

void TTP_ModelBFS( const ttp_pop_histogram_t *hist, int prefetch_distance, float mem_wait_fraction,
	ttp_model_result_t *out )
{
	uint64_t baseline_misses;
	uint64_t prefetches;
	uint64_t useful;
	uint64_t saved;
	float coverage;
	float accuracy;

	if ( !out ) {
		return;
	}

	out->baseline_miss_rate = 0.0f;
	out->ttp_miss_rate = 0.0f;
	out->coverage = 0.0f;
	out->accuracy = 0.0f;
	out->speedup = 1.0f;
	out->prefetches_issued = 0u;
	out->prefetches_useful = 0u;

	if ( !hist || hist->total_pops == 0u ) {
		return;
	}

	if ( prefetch_distance < 1 ) {
		prefetch_distance = 1;
	}

	/* BFS: the paper sweeps N={1,2,4} and shows strong but saturating gains (§VI-C). */
	baseline_misses = hist->total_pops;
	coverage = TTP_BFSCoverageForDistance( prefetch_distance );
	useful = (uint64_t)( (float)hist->total_pops * coverage );
	if ( useful > baseline_misses ) {
		useful = baseline_misses;
	}
	saved = useful;
	accuracy = 0.97f - 0.015f * (float)( prefetch_distance - 1 );
	if ( accuracy < 0.90f ) {
		accuracy = 0.90f;
	}
	prefetches = TTP_RoundUpRatio( useful, accuracy );
	if ( prefetches < useful ) {
		prefetches = useful;
	}

	out->baseline_miss_rate = 1.0f;
	out->prefetches_issued = prefetches;
	out->prefetches_useful = useful;
	out->coverage = ( baseline_misses > 0u ) ? (float)saved / (float)baseline_misses : 0.0f;
	out->accuracy = ( prefetches > 0u ) ? (float)useful / (float)prefetches : 0.0f;
	out->ttp_miss_rate = 1.0f - out->coverage;
	out->speedup = TTP_SpeedupFromCoverage( out->coverage, mem_wait_fraction );
}
